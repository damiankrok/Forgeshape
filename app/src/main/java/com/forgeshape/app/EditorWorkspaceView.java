package com.forgeshape.app;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.os.Build;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.view.WindowInsets;
import android.view.inputmethod.InputMethodManager;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;

/**
 * The whole ForgeShape editor UI.
 *
 * <p><b>The viewport is the workspace; everything else is an edge.</b> One
 * shell, four regions — the Global Toolbar along the top, the Objects capsule
 * low on the leading edge, the tool cluster on the trailing edge (the Tool Rail
 * and the precision toggle attached under it, plus the direct brush controls
 * opposite in Sculpt), and the contextual surfaces those controls open. Mode
 * and tool decide what those regions <i>contain</i>; they never decide what
 * regions there are. That is what makes the four sculpt brushes and a numeric
 * CAD inspector the same application rather than two.
 *
 * <p><b>Nothing is anchored to the bottom of the window at rest.</b> The
 * exact-value panel used to be, collapsed, and a collapsed panel is still a
 * full-width strip claiming the edge of a viewport-first tool for a surface
 * nobody asked for. Every context surface in the workspace is now opened from a
 * control and <b>grows out of that control</b> — the scene list and the Add
 * Primitive palette out of the Objects capsule, the precision surface out of
 * the rail's toggle — so what the surface is FOR is visible in where it came
 * from, and the resting workspace is the model with its tools around it.
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
        ToolRailView.OnToolSelected, PropertyInspectorView.OnPrecisionSurfaceClosed,
        DisplaySettingsPopoverView.OnDisplaySettingChanged,
        ObjectsCapsuleView.OnObjectsCapsuleAction,
        AddPrimitivePaletteView.OnPrimitiveChosen,
        StartChooserView.OnStartFlowChosen {

    private static final int[] SCULPT_TOOL_HINTS = {
            R.string.hint_grab, R.string.hint_clay, R.string.hint_smooth, R.string.hint_inflate
    };
    private static final int[] SCULPT_TOOL_NAMES = {
            R.string.tool_grab, R.string.tool_clay, R.string.tool_smooth, R.string.tool_inflate
    };

    /** The six primitives by name, indexed by {@code PRIMITIVE_*}, for the one
     *  message the Add Primitive palette writes. */
    private static final int[] PRIMITIVE_NAMES = {
            R.string.primitive_box, R.string.primitive_cylinder, R.string.primitive_sphere,
            R.string.primitive_cone, R.string.primitive_capsule, R.string.primitive_plane
    };

    /** Adopted from the workspace a theme change destroyed, when there was one;
     *  otherwise fresh. See {@link EditorUiState#forNewWorkspace()}. */
    private final EditorUiState uiState = EditorUiState.forNewWorkspace();

    private final View viewport;

    /** Last active body this chrome refreshed for; see the gesture listener. */
    private long lastKnownActiveBodyId = NativeViewport.sceneActiveBodyId();
    private final LinearLayout chromeRoot;
    private final LinearLayout middleRow;

    /**
     * The workspace's bottom edge: the Objects capsule, and — in Construction —
     * the history capsule opposite it.
     *
     * <p>It wraps its content and holds a capsule at each END with the model
     * showing between them, which is the whole difference between this and what
     * it replaced. A row that spanned the window would be a bar whatever it
     * held, and the bottom edge of a viewport-first tool is the last place to
     * put one — it is where the model is closest to the thumb and where a Sculpt
     * stroke most often begins.
     */
    private final LinearLayout bottomRow;

    /**
     * Undo and Redo, in one capsule at the trailing end of the bottom row.
     *
     * <p><b>Why not the Global Toolbar's utility group.</b> That is where
     * mode-independent global chrome lives and it is the first place these
     * belong by category — but the toolbar row is already the tightest thing in
     * the workspace. On the narrowest window the product supports, the utility
     * group, the status capsule and the editing group divide 395 dp between
     * them, and two more 48 dp controls would leave the mode transition under
     * its natural width, which is how "Back to Constructi…" happened before.
     * Buying a place for Undo by abbreviating the way out of Sculpt Mode is a
     * bad trade.
     *
     * <p>The bottom row had room and, more importantly, is where the hand is:
     * Undo is the most repeated act in an editor and the top trailing corner is
     * the hardest point on a phone to reach. It sits opposite the Objects
     * capsule, so the bottom edge reads as what the project IS on the leading
     * side and what just happened to it on the trailing one, with the model
     * between. Neither capsule spans, so this is not a bottom toolbar.
     */
    private final LinearLayout historyGroup;
    private final ImageView undoAction;
    private final ImageView redoAction;
    private final FrameLayout overlayRoot;

    private final GlobalToolbarView toolbar;
    private final ToolRailView toolRail;
    private final ScrollView toolRailScroll;

    /**
     * The trailing tool cluster: the rail, with the precision toggle attached
     * directly under it.
     *
     * <p>They are one column rather than two surfaces because they are one
     * thought — the tool that is held, and the exact values behind it. The
     * toggle's meaning is entirely a function of the entry above it, and the
     * surface it opens grows out of it, so putting it anywhere else would make
     * the relation something to be remembered rather than seen.
     */
    private final LinearLayout railColumn;

    /** The capsule the precision toggle sits in, so it wears the same floating
     *  material as the rail above it rather than standing bare on the model. */
    private final LinearLayout precisionGroup;
    private final ImageView precisionToggle;

    private final BrushEdgeControlsView brushControls;
    private final PropertyInspectorView inspector;
    private final ImageView restoreChip;
    private final DisplaySettingsPopoverView displayPopover;
    private final StartChooserView startChooser;

    /**
     * The leading-edge column an expanded window gives the scene list.
     *
     * <p>It is a host, not a second Objects implementation: the one
     * {@link ObjectsSectionView} this workspace owns is re-parented into it and
     * back out to {@link ObjectsPopoverView}. There is deliberately no second
     * list, no second row set and no Java-side copy of an {@code ObjectId} or
     * of which body is active — every row is built by re-reading native scene
     * state, in whichever host the section currently hangs.
     *
     * <p>It scrolls, because a scene can grow without limit while a window
     * cannot, and its scroll is its own: no Objects list is ever nested inside
     * the Property Inspector's scroll container.
     */
    private final ScrollView objectsDock;

    /**
     * The one scene list in the product.
     *
     * <p>Owned here rather than by the Construction shape editor, because it is
     * a <b>scene-level</b> concern and the shape editor is one of three places
     * it can be parented. It used to be built and held by that editor, which
     * made a panel named "Shape" open on the list of bodies and pushed the
     * dimensions below the fold — and made the list unreachable in any window
     * where that editor was not the inspector's current body.
     *
     * <p>Still exactly one instance, moved between hosts and never copied: a
     * second Objects view would be a second place for "which body is active" to
     * be remembered, and that answer lives below JNI.
     */
    private final ObjectsSectionView objectsSection;

    /** Where the scene list lives in every window that has no column for it. */
    private final ObjectsPopoverView objectsPopover;

    /**
     * The resting scene control: which body is active, and the {@code +}.
     *
     * <p>On screen in <b>both</b> modes and in the same place, which is most of
     * what makes Construction and Sculpt read as one workspace rather than two
     * applications sharing a viewport. Withdrawn only when the window is wide
     * enough to give the scene a permanent column, for the same reason the
     * toolbar's Objects control used to be: a capsule naming the active body
     * beside a list that already names it is one fact drawn twice.
     */
    private final ObjectsCapsuleView objectsCapsule;

    /**
     * The one creation surface, shared by both {@code +} controls.
     *
     * <p>One surface rather than one per host, for exactly the reason there is
     * one Objects section: creation routes through one place, so the phone and
     * the tablet cannot drift into creating bodies differently.
     */
    private final AddPrimitivePaletteView addPrimitivePalette;

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

    /** False while a viewport gesture is in flight; see the gesture listener.
     *  Chrome transitions are instant during one, because pointer samples
     *  outrank motion. */
    private boolean chromeMotionAllowed = true;

    /** What the precision surface is currently placed as, so it is re-parented
     *  only when the answer actually changed. */
    private WorkspaceLayoutMode.InspectorPlacement appliedPlacement;
    private int appliedInspectorWidthPx;

    /** Whether Objects currently has a column of its own, so the section is
     *  re-parented only when the answer actually changed. Boxed so the first
     *  decision is always applied, whichever way it goes. */
    private Boolean appliedObjectsDocked;

    /**
     * Whether the current WINDOW could afford an Objects column.
     *
     * <p>Kept separately from {@link #appliedObjectsDocked} because the answer
     * has two halves that change at different times: the window's half moves on
     * a resize or a rotation, and the mode's half moves when the user starts or
     * stops sculpting. Remembering the window's answer is what lets a mode
     * change re-run the decision without re-running the whole adaptive pass.
     * See {@link #applyObjectsPlacement()}.
     */
    private boolean objectsColumnAffordable;

    /** Whether the Tool Rail is currently drawn flush rather than floating. */
    private Boolean appliedRailDocked;

    EditorWorkspaceView(Context context, View viewport) {
        super(context);
        this.viewport = viewport;
        setId(R.id.editor_workspace);

        // Child 0: the viewport, at the whole window size, under everything.
        addView(viewport, new LayoutParams(LayoutParams.MATCH_PARENT,
                LayoutParams.MATCH_PARENT));

        // A tap in the viewport can change which body the Construction editors
        // act on, so the chrome has to re-read when a gesture settles. The
        // check is deliberately "did the active body actually change": an orbit,
        // a pan and a tap that hit nothing all end here and cost nothing.
        if (viewport instanceof ForgeShapeSurfaceView) {
            ((ForgeShapeSurfaceView) viewport).setOnViewportGestureSettled(
                    new ForgeShapeSurfaceView.OnViewportGestureSettled() {
                        @Override
                        public void onViewportGestureStarted() {
                            // The user's pointer is on the model — in Sculpt
                            // Mode that is a real stroke. Chrome must not spend
                            // main-thread time on a transition while pointer
                            // samples are arriving, so from here until the
                            // gesture settles every chrome detent change is
                            // instant. Nothing about the gesture's own
                            // arbitration is touched: this decides only whether
                            // a PANEL animates.
                            setChromeMotionAllowed(false);
                        }

                        @Override
                        public void onViewportGestureSettled() {
                            setChromeMotionAllowed(true);
                            final long active = NativeViewport.sceneActiveBodyId();
                            if (active != lastKnownActiveBodyId) {
                                lastKnownActiveBodyId = active;
                                onNativeStateChanged();
                            }
                        }
                    });
        }

        // Child 1: all interactive chrome. This container is transparent and
        // not clickable, so a touch in the gaps between chrome surfaces falls
        // through to the viewport exactly as it should; only the surfaces
        // themselves are opaque to touch.
        chromeRoot = new LinearLayout(context);
        chromeRoot.setOrientation(LinearLayout.VERTICAL);
        // Floating chrome casts a shadow OUTSIDE its own bounds, so a clipping
        // container would remove exactly the part that makes the surface look
        // raised. Nothing here changes what is touchable: a shadow is drawn,
        // never hit-tested.
        EditorControlStyles.allowChildShadows(chromeRoot);
        addView(chromeRoot, new LayoutParams(LayoutParams.MATCH_PARENT,
                LayoutParams.MATCH_PARENT));

        toolbar = new GlobalToolbarView(context, this);
        chromeRoot.addView(toolbar, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        middleRow = new LinearLayout(context);
        middleRow.setOrientation(LinearLayout.HORIZONTAL);
        EditorControlStyles.allowChildShadows(middleRow);
        chromeRoot.addView(middleRow, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1.0f));

        // FIRST in the middle row, so the scene list is on the leading edge and
        // the exact-value inspector on the trailing one: what is being edited on
        // the left, what its numbers are on the right, the model between them.
        // It carries no width until the layout decision gives it one, and is
        // GONE in every window that has not earned it.
        objectsDock = new ScrollView(context);
        objectsDock.setId(R.id.objects_dock);
        objectsDock.setVisibility(GONE);
        // TIER 2, INSET — the same material the Objects PANEL wears on a phone,
        // and deliberately not a docked slab any more.
        //
        // It used to be bg_chrome_docked_leading: opaque, flat, square against
        // the window edge and rounded only on the side facing the model. That is
        // desktop-CAD furniture. It made the expanded window a different product
        // from the phone rather than the same product with more room — the same
        // scene list, in the same session, wore a floating rounded surface on one
        // window and a wall on the other. Here it is a context surface standing
        // clear of the edge, which is the vocabulary every other surface in the
        // workspace already speaks.
        EditorControlStyles.applyContextSurface(objectsDock);
        // Opaque to touch, exactly as every other chrome surface is, so reaching
        // for a body never orbits the camera behind the column. A clickable
        // ScrollView consumes what its own scrolling and its own rows did not.
        objectsDock.setClickable(true);
        // The same padding the Property Inspector uses, because the two are the
        // two docked columns of the same layout and a 4 dp inset put the OBJECTS
        // heading hard against the window edge.
        final int dockPad = EditorControlStyles.dimen(context, R.dimen.inspector_padding);
        objectsDock.setPadding(dockPad, dockPad, dockPad, dockPad);
        // Top-aligned and WRAPPING its content, for the same reason the side
        // inspector does: a scene of two bodies in a full-height column is one
        // short list and an arm's length of empty panel. Bounded by the row, so
        // a scene that outgrows the window scrolls instead of stretching it.
        final LinearLayout.LayoutParams objectsParams = new LinearLayout.LayoutParams(
                0, ViewGroup.LayoutParams.WRAP_CONTENT);
        objectsParams.gravity = Gravity.TOP;
        objectsParams.rightMargin = EditorControlStyles.dimen(context, R.dimen.row_gap_small);
        // INSET from the leading window edge, not flush against it. This is the
        // half of "same vocabulary" that is a margin rather than a material: a
        // surface touching the window edge is part of the frame, and a surface
        // standing off it is a panel in a workspace. It is the same claim the
        // inset bottom sheet already makes.
        objectsParams.leftMargin = EditorControlStyles.dimen(context, R.dimen.row_gap);
        // Clear of the status capsule above it. The toolbar container is
        // transparent, so without this the column's top edge butts straight into
        // a floating capsule and the two read as one broken surface.
        objectsParams.topMargin = EditorControlStyles.dimen(context, R.dimen.row_gap);
        // And clear of the bottom edge, so it is a bounded panel rather than a
        // column that happens to be short.
        objectsParams.bottomMargin = EditorControlStyles.dimen(context, R.dimen.row_gap);
        middleRow.addView(objectsDock, objectsParams);

        brushControls = new BrushEdgeControlsView(context);
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
        // The scroll container carries the rail's floating surface and its
        // depth, because it is the view whose bounds the rail actually
        // occupies. Putting them on the rail itself would have the container
        // clip the shadow away. See ToolRailView's constructor.
        EditorControlStyles.applyFloatingSurface(toolRailScroll);
        toolRailScroll.addView(toolRail, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        // The rail and its precision toggle are one trailing cluster, so they
        // are one column. The toggle is a separate capsule rather than a fifth
        // rail entry because it is not a tool: the rail says WHICH tool is
        // held, and this says "show me the numbers behind it". Making it look
        // like an entry would put a fifth selectable thing in a control whose
        // whole job is that exactly one of its children is active.
        railColumn = new LinearLayout(context);
        railColumn.setOrientation(LinearLayout.VERTICAL);
        railColumn.setGravity(Gravity.END);
        EditorControlStyles.allowChildShadows(railColumn);
        railColumn.addView(toolRailScroll, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        precisionGroup = EditorControlStyles.controlGroup(context);
        precisionToggle = EditorControlStyles.iconButton(context, R.id.precision_toggle,
                R.drawable.ic_precision, context.getString(R.string.precision_shape));
        precisionToggle.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                onPrecisionToggleRequested();
            }
        });
        precisionGroup.addView(precisionToggle,
                EditorControlStyles.iconButtonParams(context, 0));
        final LinearLayout.LayoutParams precisionParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        precisionParams.topMargin =
                EditorControlStyles.dimen(context, R.dimen.row_gap_small);
        railColumn.addView(precisionGroup, precisionParams);

        final LinearLayout.LayoutParams railParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        railParams.gravity = Gravity.CENTER_VERTICAL;
        railParams.rightMargin = EditorControlStyles.dimen(context, R.dimen.brush_gap);
        middleRow.addView(railColumn, railParams);

        // The bottom edge: one capsule, wrapping its own content, on the
        // leading side. Added to the chrome root AFTER the weighted middle row
        // and BEFORE the precision surface's bottom-sheet placement, so an open
        // sheet stands below the capsule rather than covering it.
        bottomRow = new LinearLayout(context);
        bottomRow.setOrientation(LinearLayout.HORIZONTAL);
        bottomRow.setGravity(Gravity.CENTER_VERTICAL);
        EditorControlStyles.allowChildShadows(bottomRow);
        final int edgePad = EditorControlStyles.dimen(context, R.dimen.row_gap);
        bottomRow.setPadding(edgePad, 0, edgePad, edgePad);
        chromeRoot.addView(bottomRow, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        objectsCapsule = new ObjectsCapsuleView(context, this);
        bottomRow.addView(objectsCapsule, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        // The flexible child of the bottom row, and the only one: it is where
        // the model shows between the two capsules, and it is what absorbs a
        // squeeze, so neither capsule ever gives up a touch target.
        bottomRow.addView(EditorControlStyles.spacer(context));

        historyGroup = EditorControlStyles.controlGroup(context);
        historyGroup.setId(R.id.history_group);
        undoAction = EditorControlStyles.iconButton(context, R.id.undo_action,
                R.drawable.ic_undo, context.getString(R.string.undo));
        undoAction.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                onHistoryStepRequested(false);
            }
        });
        historyGroup.addView(undoAction, EditorControlStyles.iconButtonParams(context, 0));
        redoAction = EditorControlStyles.iconButton(context, R.id.redo_action,
                R.drawable.ic_redo, context.getString(R.string.redo));
        redoAction.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                onHistoryStepRequested(true);
            }
        });
        historyGroup.addView(redoAction, EditorControlStyles.iconButtonParams(context,
                EditorControlStyles.dimen(context, R.dimen.toolbar_gap)));
        bottomRow.addView(historyGroup, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        inspector = new PropertyInspectorView(context, this);

        // Child 2: surfaces that must survive the chrome being hidden.
        overlayRoot = new FrameLayout(context);
        EditorControlStyles.allowChildShadows(overlayRoot);
        addView(overlayRoot, new LayoutParams(LayoutParams.MATCH_PARENT,
                LayoutParams.MATCH_PARENT));
        restoreChip = EditorControlStyles.iconButton(context, R.id.restore_ui_chip,
                R.drawable.ic_chrome_show, context.getString(R.string.show_ui));
        restoreChip.setElevation(
                EditorControlStyles.dimen(context, R.dimen.elevation_floating));
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

        // Built before the editors that used to own it, and parented by the
        // layout decision rather than here: until applyLayoutForWindow runs it
        // belongs to no host, which is exactly what makes "one instance, three
        // hosts" true rather than aspirational.
        objectsSection = new ObjectsSectionView(context, this);
        objectsPopover = new ObjectsPopoverView(context);
        overlayRoot.addView(objectsPopover, ObjectsPopoverView.anchoredParams(context));

        // The one creation surface, in the overlay for the same reason the two
        // panels above it are: it has to be able to stand over the chrome that
        // opened it without becoming part of a measured row.
        addPrimitivePalette = new AddPrimitivePaletteView(context, this);
        overlayRoot.addView(addPrimitivePalette,
                AddPrimitivePaletteView.anchoredParams(context));

        shapeEditor = new ConstructionShapeEditorView(context, this);
        placementEditor = new ConstructionPlacementEditorView(context, this);
        sculptContext = new SculptContextView(context, this);

        // Last into the overlay, so the question is above everything it is
        // asking about. It stands on a WORKING workspace: native state already
        // exists (the Activity starts native code before building any view),
        // the default Body is already there, and the viewport is already
        // rendering it behind the scrim. Choosing Construction therefore has
        // nothing to build — it only stops asking.
        startChooser = new StartChooserView(context, this);
        overlayRoot.addView(startChooser, new LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));

        installInsetListener();
        syncFromNative();
        showStartChooser(!uiState.startChoiceMade());
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

        toolbar.setStatusInline(WorkspaceLayoutMode.statusInlineWithControls(heightDp));
        // A compact window cannot fit the context label AND every global action
        // above the touch floor, and the label is the one of the three
        // context-bearing surfaces that a narrow row squeezes to nothing. See
        // GlobalToolbarView#setContextLabelVisible.
        toolbar.setContextLabelVisible(layoutMode != WorkspaceLayoutMode.COMPACT);
        toolRail.setCompactEntries(heightDp < WorkspaceLayoutMode.LOW_HEIGHT_MAX_DP);
        // Roughly half the window's height for the two brush tracks, bounded by
        // the control's own sensible range, so they shrink with the window
        // instead of being clipped by it.
        brushControls.setTrackHeightPx(Math.round(heightPx * 0.45f));

        objectsColumnAffordable = layoutMode.objectsDocked(widthDp);
        applyObjectsPlacement();
        applyRailDock(layoutMode.railDocked());
        placeInspector(layoutMode.inspectorPlacement(heightDp), widthDp, heightDp);
    }

    /**
     * Decides whether Objects has a column right now, from the window <b>and</b>
     * the mode.
     *
     * <p>A window wide enough is necessary and no longer sufficient. In Sculpt a
     * permanent scene column is not context, it is furniture: body switching is
     * refused while sculpting, creation is refused, and what is left is a list
     * of names with nothing to do — 180 dp of the leading edge spent on it.
     *
     * <p>And it was not free. The column sits before the brush controls in the
     * middle row, so its width pushed Radius and Strength 180 dp inboard, onto
     * the model and out from under the hand that reaches for them. That is the
     * expanded-window defect the owner review named: the two most-used Sculpt
     * controls displaced by a panel that Sculpt cannot use. Withdrawing the
     * column in Sculpt answers both at once — the brush controls return to the
     * leading edge, and Sculpt gets the same Objects capsule the phone has, in
     * the same place, which is the vocabulary the expanded window is supposed to
     * share.
     *
     * <p>The scene stays reachable in every window and every mode: the capsule
     * names the active body and opens the list.
     */
    private void applyObjectsPlacement() {
        placeObjects(objectsColumnAffordable && !isSculpting());
    }

    /**
     * Gives Objects a column of its own, or hands it back to the shape editor.
     *
     * <p><b>One instance, re-parented</b> — never a second list. A second Java
     * Objects view would be a second place for "which body is active" to be
     * remembered, and the answer to that question lives in exactly one place,
     * below JNI. Because the same view moves, a viewport pick, an Objects row
     * tap and Add Body all still end at the same one native fact and the same
     * one {@code refreshFromNative()}, whichever window the user is in.
     *
     * <p>Instant, not animated. This runs inside {@code onMeasure}, and
     * starting an animation from a measure pass is how a re-parented surface
     * ends up laid out at zero height; a structural rearrangement caused by a
     * rotation or a window resize is also not a transition a user asked for.
     * {@link ChromeMotion} is untouched.
     */
    private void placeObjects(boolean docked) {
        if (appliedObjectsDocked != null && appliedObjectsDocked == docked) {
            return;
        }
        appliedObjectsDocked = docked;

        // The capsule exists only while the panel is the way to reach the
        // scene. A docked column IS the scene, permanently, and names the
        // active body itself — so a capsule beside it would draw one fact
        // twice. GONE rather than invisible: the capsule then costs the row no
        // width at all, so the leading end of the bottom edge gives the model
        // back everything the capsule was standing on. The history capsule at
        // the trailing end is unaffected and stays: which body is current has a
        // second home on a docked window, and taking a change back does not.
        objectsCapsule.setVisibility(docked ? GONE : VISIBLE);

        if (!docked) {
            objectsDock.setVisibility(GONE);
            final ViewGroup.LayoutParams gone = objectsDock.getLayoutParams();
            if (gone != null && gone.width != 0) {
                gone.width = 0;
                objectsDock.setLayoutParams(gone);
            }
            objectsPopover.host(objectsSection);
            objectsSection.refreshFromNative();
            return;
        }
        // Losing the capsule must also close the surfaces it opened, or a
        // window that grows would leave two copies of the same list on screen
        // and a palette anchored to a control that is no longer there.
        setObjectsPanelOpen(false);
        setAddPrimitiveOpen(false, null);
        if (objectsSection.getParent() instanceof ViewGroup) {
            ((ViewGroup) objectsSection.getParent()).removeView(objectsSection);
        }
        objectsDock.addView(objectsSection, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        final ViewGroup.LayoutParams params = objectsDock.getLayoutParams();
        if (params != null) {
            params.width = dpToPx(WorkspaceLayoutMode.OBJECTS_DOCK_WIDTH_DP);
            objectsDock.setLayoutParams(params);
        }
        objectsDock.setVisibility(VISIBLE);
        // The column is built from whatever the scene currently holds. The
        // section was possibly last refreshed while it lived somewhere else, and
        // a rebuild is a handful of rows.
        objectsSection.refreshFromNative();
    }

    /**
     * Opens or closes the Objects panel, and keeps its opener in step.
     *
     * <p>Refuses to open it when the window has a column instead: there would be
     * two copies of one list on screen, and the panel would be standing on the
     * model for no reason.
     */
    private void setObjectsPanelOpen(boolean open) {
        if (open && objectsDocked()) {
            return;
        }
        if (open) {
            // The capsule's two surfaces are alternatives, not a stack, and the
            // Display popover is a third: opening any one closes the others.
            displayPopover.setOpen(false);
            toolbar.showDisplaySettingsOpen(false);
            setAddPrimitiveOpen(false, null);
            objectsSection.refreshFromNative();
            anchorOverlayTo(objectsPopover, objectsCapsule);
        }
        objectsPopover.setOpen(open);
        // Drawn from what was ASKED FOR, not from what the panel currently
        // looks like: a closing panel is still visible for the length of its
        // fade, and a control that read that would stay lit after the surface
        // had gone.
        objectsCapsule.showObjectsOpen(objectsPopover.isOpen());
    }

    // -----------------------------------------------------------------------
    // Creating a body
    // -----------------------------------------------------------------------

    /**
     * Opens the Add Primitive palette out of whichever plus was pressed.
     *
     * <p>Anchored to the invoking control rather than to a window edge, because
     * there are two of them — the Objects capsule's on a phone, the docked
     * column's on a tablet — and a surface that appeared in the same corner
     * whichever was pressed would say nothing about what it belongs to.
     */
    @Override
    public void onAddPrimitiveRequested(View invoker) {
        setAddPrimitiveOpen(!addPrimitivePalette.isOpen(), invoker);
    }

    /** The Objects capsule's own plus. */
    @Override
    public void onAddPrimitiveRequested() {
        onAddPrimitiveRequested(objectsCapsule.addControl());
    }

    private void setAddPrimitiveOpen(boolean open, View invoker) {
        if (open) {
            // One surface at a time, exactly as the scene list and the Display
            // popover are.
            setObjectsPanelOpen(false);
            displayPopover.setOpen(false);
            toolbar.showDisplaySettingsOpen(false);
            anchorOverlayTo(addPrimitivePalette,
                    invoker != null ? invoker : objectsCapsule.addControl());
        }
        addPrimitivePalette.setOpen(open);
        objectsCapsule.showAddOpen(addPrimitivePalette.isOpen());
    }

    /**
     * Creates a body and makes it the chosen primitive, through the product's
     * own two entry points and nothing else.
     *
     * <p><b>Every step is an existing, verified path.</b> The scene's own
     * {@code sceneAddBody()} creates and selects; then that primitive's own
     * {@code applyConstruction*()} is called with the parameters <b>read back
     * from the new body's own native state</b>, so the sizes are the domain's
     * canonical defaults rather than constants invented in Java. Nothing about
     * primitive construction, defaulting or validation is duplicated here, and
     * a shape chosen from a palette can be refused exactly as a typed one can.
     *
     * <p>{@code UNCHANGED} is a success: a newly added body is already a box,
     * so choosing Box legitimately changes nothing.
     *
     * <p>On a refusal the body still exists and is still a box, and the status
     * line says exactly that. It is deliberately not repaired and the body is
     * deliberately not removed — there is no scene delete in the product, and
     * inventing one to tidy up after a refusal would put a destructive path
     * into the shell for the sake of a tidier message.
     */
    @Override
    public void onAddPrimitiveChosen(int primitiveKind) {
        final Context context = getContext();
        final long created;
        // Only meaningful when a body was actually created; the refusal below
        // returns before reading it.
        int applied = NativeViewport.APPLY_APPLIED;
        // ONE Construction edit around both native calls, so choosing Sphere is
        // one history step rather than two — and so undoing it removes the body
        // outright instead of leaving behind the default Box the append produces
        // before the primitive is applied. The commit compares state, so a
        // refused add records nothing at all.
        //
        // The begin/commit pair is balanced through a finally, because a step
        // left open would silently absorb the user's next edit into this one.
        final boolean owned = NativeViewport.beginConstructionEdit();
        try {
            created = NativeViewport.sceneAddBody();
            if (created != 0L) {
                applied = applyPrimitiveToActiveBody(primitiveKind);
            }
        } finally {
            if (owned) {
                NativeViewport.commitConstructionEdit();
            }
        }
        if (created == 0L) {
            // The only refusal is "not while sculpting".
            showStatus(context.getString(R.string.status_body_add_failed), R.attr.fsTextError);
            setAddPrimitiveOpen(false, null);
            return;
        }
        final boolean shaped = applied == NativeViewport.APPLY_APPLIED
                || applied == NativeViewport.APPLY_UNCHANGED;

        // The palette has done its work; the focus belongs back on the model,
        // which is where the new body now is.
        setAddPrimitiveOpen(false, null);
        finishEditing();
        onNativeStateChanged();

        final String body = context.getString(R.string.body_label, created);
        final String kind = context.getString(PRIMITIVE_NAMES[primitiveKind]);
        showStatus(context.getString(shaped ? R.string.status_body_created
                        : R.string.status_body_created_unshaped, body, kind),
                shaped ? R.attr.fsTextSuccess : R.attr.fsTextError);
    }

    /**
     * Submits one primitive through its own native method, with that
     * primitive's own parameters read back from the active body.
     *
     * <p>Each branch reads exactly the slots that primitive has and calls the
     * method that takes exactly those parameters, so there is no point at which
     * a value is carried in a slot whose meaning depends on a separate kind —
     * the same rule the shape editor's Apply follows.
     */
    private int applyPrimitiveToActiveBody(int primitiveKind) {
        final double[] primitive = new double[NativeViewport.PRIMITIVE_STATE_SIZE];
        NativeViewport.constructionPrimitive(primitive);
        switch (primitiveKind) {
            case NativeViewport.PRIMITIVE_CYLINDER:
                return NativeViewport.applyConstructionCylinder(
                        primitive[NativeViewport.PRIMITIVE_CYLINDER_DIAMETER],
                        primitive[NativeViewport.PRIMITIVE_CYLINDER_DIAMETER + 1]);
            case NativeViewport.PRIMITIVE_SPHERE:
                return NativeViewport.applyConstructionSphere(
                        primitive[NativeViewport.PRIMITIVE_SPHERE_DIAMETER]);
            case NativeViewport.PRIMITIVE_CONE:
                return NativeViewport.applyConstructionCone(
                        primitive[NativeViewport.PRIMITIVE_CONE_BOTTOM_DIAMETER],
                        primitive[NativeViewport.PRIMITIVE_CONE_BOTTOM_DIAMETER + 1]);
            case NativeViewport.PRIMITIVE_CAPSULE:
                return NativeViewport.applyConstructionCapsule(
                        primitive[NativeViewport.PRIMITIVE_CAPSULE_DIAMETER],
                        primitive[NativeViewport.PRIMITIVE_CAPSULE_DIAMETER + 1]);
            case NativeViewport.PRIMITIVE_PLANE:
                return NativeViewport.applyConstructionPlane(
                        primitive[NativeViewport.PRIMITIVE_PLANE_WIDTH],
                        primitive[NativeViewport.PRIMITIVE_PLANE_WIDTH + 1]);
            default:
                return NativeViewport.applyConstructionBox(
                        primitive[NativeViewport.PRIMITIVE_BOX_WIDTH],
                        primitive[NativeViewport.PRIMITIVE_BOX_WIDTH + 1],
                        primitive[NativeViewport.PRIMITIVE_BOX_WIDTH + 2]);
        }
    }

    /**
     * Anchors a context surface to the control it grew out of.
     *
     * <p>The rule is the one thing every surface the workspace opens has in
     * common: it appears beside the control that opened it and unfolds from
     * that corner. So the position is arithmetic on the invoker's bounds rather
     * than a gravity chosen per panel — the Objects capsule is low in the window
     * on a phone and its plus sits under a column on a tablet, and both have to
     * work.
     *
     * <p>The surface's own width is read from its layout params, because this
     * runs before it has ever been measured. Coordinates come out of the
     * overlay's inset padding, since the chrome container the invoker lives in
     * carries the same padding.
     */
    private void anchorOverlayTo(View overlay, View invoker) {
        if (invoker == null || invoker.getWidth() <= 0
                || getWidth() <= 0 || getHeight() <= 0) {
            return;
        }
        final Rect bounds = new Rect(0, 0, invoker.getWidth(), invoker.getHeight());
        offsetDescendantRectToMyCoords(invoker, bounds);

        final FrameLayout.LayoutParams params =
                (FrameLayout.LayoutParams) overlay.getLayoutParams();
        final int gap = EditorControlStyles.dimen(getContext(), R.dimen.overlay_anchor_gap);
        final int width = params.width > 0 ? params.width : overlay.getWidth();

        // Above the invoker when it sits in the lower half of the window, below
        // it otherwise: a surface must never have to grow off the edge it is
        // nearest to.
        final boolean upward = bounds.centerY() > getHeight() / 2;
        params.gravity = (upward ? Gravity.BOTTOM : Gravity.TOP) | Gravity.START;
        params.leftMargin = Math.max(0,
                Math.min(bounds.left, trailingLimitFor(invoker, width, gap) - width)
                        - overlayRoot.getPaddingLeft());
        params.rightMargin = 0;
        params.topMargin = upward ? 0
                : Math.max(0, bounds.bottom + gap - overlayRoot.getPaddingTop());
        params.bottomMargin = upward
                ? Math.max(0, getHeight() - bounds.top + gap - overlayRoot.getPaddingBottom())
                : 0;
        overlay.setLayoutParams(params);

        if (overlay == objectsPopover) {
            objectsPopover.setGrowsUpward(upward);
        } else if (overlay == addPrimitivePalette) {
            addPrimitivePalette.setGrowsUpward(upward);
        } else if (overlay == inspector) {
            inspector.setGrowsUpward(upward);
        }
    }

    /**
     * How far right an anchored surface may reach before it starts covering a
     * control that is still live underneath it.
     *
     * <p>A context surface may stand on the model — that is what makes it a
     * surface over a viewport rather than a second column — but it may not stand
     * on <b>another control</b>. Add Primitive did: anchored to the Objects
     * capsule's {@code +} low on the leading edge, it is wider than the distance
     * from that {@code +} to the window edge, so the old clamp — "as far right
     * as the window allows" — slid it under the trailing tool cluster and left a
     * crescent of the precision toggle sticking out from behind it. A control
     * half-covered by a panel is worse than one that is not drawn: it still
     * takes a touch, and the panel above it reads as broken rather than as
     * layered.
     *
     * <p>So the limit is the tool cluster's own leading edge, less the same gap
     * every anchored surface already stands off its invoker. The palette moves
     * rather than the cluster, and it still grows out of the {@code +} it came
     * from — nothing about the motion, the pivot or the z-order changes, because
     * hiding the collision behind a z-order is not resolving it: the control
     * would still be under the panel and still be taking touches.
     *
     * <p>The rule is skipped for a surface the cluster itself opened. The
     * precision surface's invoker IS the cluster's toggle, and a surface
     * forbidden to overlap the control it grew out of could not be anchored to
     * it at all.
     */
    private int trailingLimitFor(View invoker, int width, int gap) {
        final int windowLimit = getWidth() - gap;
        if (!railColumn.isShown() || isInTrailingCluster(invoker)) {
            return windowLimit;
        }
        final Rect cluster = new Rect(0, 0, railColumn.getWidth(), railColumn.getHeight());
        offsetDescendantRectToMyCoords(railColumn, cluster);
        // Never tighter than the surface's own width: a window too narrow to
        // seat it beside the cluster is still laid out, at the leading edge,
        // rather than at a negative margin.
        return Math.max(Math.min(windowLimit, cluster.left - gap), width);
    }

    /** Whether this control is part of the trailing tool cluster. */
    private boolean isInTrailingCluster(View control) {
        for (View view = control; view != null; ) {
            if (view == railColumn) {
                return true;
            }
            final ViewParent parent = view.getParent();
            view = parent instanceof View ? (View) parent : null;
        }
        return false;
    }

    /**
     * Draws the Tool Rail as part of the layout rather than as a surface over
     * the model — the promotion of {@link WorkspaceLayoutMode#railDocked()},
     * which had a tested meaning and had never been asked.
     *
     * <p><b>What changes is where it sits, and no longer what it is made of.</b>
     * Docking used to also repaint the rail as an opaque slab flush against the
     * window edge with no depth. That was the expanded window speaking a
     * different visual language from the phone about the same control — a
     * Sculpt user who rotated a tablet watched their brush selector turn from a
     * floating capsule into part of the wall, and it read as a desktop CAD frame
     * rather than as a viewport with its tools around it. The rail is a floating
     * capsule in every window now, and docking means only that it is
     * top-aligned with the panel beside it instead of centred on the thumb.
     *
     * <p>What does <b>not</b> change is the {@code SurfaceView}, in any mode.
     * It is the whole window in a compact portrait phone and the whole window
     * on a docked tablet; docking rearranges chrome and never the render
     * target, so nothing here can resize a swapchain.
     */
    private void applyRailDock(boolean docked) {
        if (appliedRailDocked != null && appliedRailDocked == docked) {
            return;
        }
        appliedRailDocked = docked;
        // The precision toggle keeps its floating capsule in both cases. It is
        // not part of the docked frame even when the rail above it is: it opens
        // a surface over the model, and a flush toggle hanging off the bottom of
        // a docked column would claim to be a fifth rail entry.
        final ViewGroup.LayoutParams params = railColumn.getLayoutParams();
        if (params instanceof LinearLayout.LayoutParams) {
            final LinearLayout.LayoutParams rail = (LinearLayout.LayoutParams) params;
            // The gap off the trailing edge is kept in BOTH cases now. A rail
            // flush against the window edge is the shape that made an expanded
            // window read as a frame; the same gap in every window is what makes
            // the same control recognisably the same control.
            rail.rightMargin = EditorControlStyles.dimen(getContext(), R.dimen.brush_gap);
            // What docking still decides is where the cluster starts.
            //
            // This is what makes "part of the layout" a true claim rather than a
            // style. A docked rail centred on the window height while the panel
            // beside it hangs from the top is not a layout: it is one surface
            // stranded halfway down the model, which is exactly how it read once
            // the side panels stopped spanning the full window. Top-aligned, the
            // rail and the panel are one trailing cluster. A FLOATING rail stays
            // centred, because a capsule standing on the picture belongs where
            // the thumb is, not where the chrome above it ended.
            rail.gravity = docked ? Gravity.TOP : Gravity.CENTER_VERTICAL;
            rail.topMargin =
                    docked ? EditorControlStyles.dimen(getContext(), R.dimen.row_gap) : 0;
            railColumn.setLayoutParams(rail);
        }
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
        // One surface in all three placements. A panel that becomes a flat
        // square-edged column on a tablet and a rounded card on a phone is two
        // designs for one thing, and the user meets both by rotating a device.
        // See PropertyInspectorView#showAsFloatingPanel.
        inspector.showAsFloatingPanel();
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
            // INSET, not flush. A sheet touching three window edges reads as a
            // platform bottom sheet dragged into a creative tool; the same
            // content standing off those edges, with the viewport visible around
            // it, reads as one more surface in the workspace — the same claim
            // the Tool Rail and the toolbar's control groups already make. It
            // costs a few dp of height and buys the whole composition.
            final LinearLayout.LayoutParams sheetParams = new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            final int inset = EditorControlStyles.dimen(context, R.dimen.inspector_sheet_inset);
            sheetParams.leftMargin = inset;
            sheetParams.rightMargin = inset;
            sheetParams.bottomMargin = inset;
            chromeRoot.addView(inspector, sheetParams);
            return;
        }
        // WRAP_CONTENT and top-aligned, not MATCH_PARENT.
        //
        // A side panel stretched to the full window height is mostly empty for
        // most of the product's life — a box has three dimensions, a placement
        // has six fields, and a tablet window is 2500 px tall. That emptiness is
        // what made the expanded layout read as a desktop CAD frame rather than
        // as a viewport with panels beside it. Wrapping its content means the
        // panel ends where its content ends and the viewport keeps the rest of
        // the column, which is the whole point of a viewport-first tool.
        //
        // It still cannot outgrow the window: a WRAP_CONTENT child of a bounded
        // LinearLayout is measured AT_MOST the parent's height, so a long body
        // simply scrolls exactly as it always did.
        final LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                widthPx, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.gravity = Gravity.TOP;
        params.leftMargin = EditorControlStyles.dimen(context, R.dimen.row_gap_small);
        // Clear of the toolbar's utility capsule, for the same reason the
        // Objects column is: nothing above this is an opaque strip any more.
        params.topMargin = EditorControlStyles.dimen(context, R.dimen.row_gap);
        // INSET from the trailing window edge and off the bottom, the same way
        // the Objects column stands off the leading one and the bottom sheet
        // stands off three. A panel flush against a window edge is part of the
        // frame; a panel standing off it is a surface in a workspace.
        params.rightMargin = EditorControlStyles.dimen(context, R.dimen.row_gap);
        params.bottomMargin = EditorControlStyles.dimen(context, R.dimen.row_gap);
        middleRow.addView(inspector, params);
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

        // Alpha only, and short. The viewport was always the whole window, so
        // there is no size here for a transition to change — and one that DID
        // change a size would rebuild the swapchain for a question about where
        // buttons are drawn. Both surfaces are cancelled first, so a rapid
        // hide/restore reverses rather than queueing and can never settle with
        // both the chrome and its restore affordance half visible.
        final long durationMs = ChromeMotion.duration(
                hidden ? ChromeMotion.EXIT_MS : ChromeMotion.ENTER_MS,
                chromeMotionAllowed ? ChromeMotion.animatorScale(getContext()) : 0.0f);
        ChromeMotion.fade(chromeRoot, !hidden, durationMs);
        ChromeMotion.fade(restoreChip, hidden, durationMs);
        toolbar.showChromeHidden(hidden);
        if (hidden) {
            // Hiding chrome means "show me the bare model". Both panels live in
            // the overlay, so they would otherwise survive the very act that was
            // meant to clear the viewport. Closed without animation: the chrome
            // around them is disappearing in the same frame, so animating a
            // panel out would only draw attention to it.
            displayPopover.closeImmediately();
            toolbar.showDisplaySettingsOpen(false);
            objectsPopover.closeImmediately();
            objectsCapsule.showObjectsOpen(false);
            addPrimitivePalette.closeImmediately();
            objectsCapsule.showAddOpen(false);
        }
    }

    boolean chromeHidden() {
        return uiState.chromeHidden();
    }

    // -----------------------------------------------------------------------
    // The start chooser
    // -----------------------------------------------------------------------
    //
    // Asked once per PROCESS, not once per Activity and not once per window.
    // The flag lives in EditorUiState precisely so a rotation, a HOME/resume or
    // an Activity recreation cannot put the question back — see the field's own
    // comment for why it is the one static there.
    //
    // Neither answer is a new document. Native state already exists by the time
    // this view is built (ForgeShapeActivity starts native code first), so the
    // scene, the default Body and its ObjectId are the same in both branches.
    // What the answer decides is only which representation the user lands in.

    /** Shows or hides the question. Nothing else about the workspace changes. */
    private void showStartChooser(boolean visible) {
        startChooser.setVisibility(visible ? VISIBLE : GONE);
    }

    /** Whether the start question is currently on screen. */
    boolean startChooserVisible() {
        return startChooser.getVisibility() == VISIBLE;
    }

    /**
     * Construction / CAD: the workspace the product already had.
     *
     * <p>There is deliberately nothing to build. The default Body is already in
     * the scene at identity, already selected, and already published, so this
     * only records that the question was answered and re-reads native state so
     * the exact-value editors show that body's own numbers.
     *
     * <p>And it says nothing. Nothing happened that the user did not just do,
     * and the workspace behind the question already answers where they are —
     * the held rail entry, the body named on the Objects capsule, the model
     * itself. It used to write "Construction — choose a shape, type its exact
     * values, then Apply.", which is a caption for the product rather than a
     * verdict about an act, and it opened every resting Construction screenshot
     * with a sentence across the top of the viewport.
     */
    @Override
    public void onConstructionStartChosen() {
        uiState.recordStartChoice();
        showStartChooser(false);
        syncFromNative();
    }

    /**
     * Sculpt: land directly on a mesh that is ready for a brush.
     *
     * <p><b>Every step here is an existing, verified product entry point.</b>
     * Nothing about Freeze is duplicated or re-implemented: no second
     * validation, no second {@code SculptMesh} construction, no opinion about
     * sidedness, the stale-source flag or revision numbering. This method makes
     * exactly the two calls a user would make by hand, in the order they would
     * make them, and then reads back what native code decided.
     *
     * <p>The diameter comes from native state rather than from a constant
     * invented here. Every body remembers each primitive's parameters
     * independently, so what is read back is the domain's own canonical default
     * sphere — and if the user has already sized a sphere on this body, that is
     * what they get, which is the correct answer and not a special case.
     *
     * <p>The Construction Source is not consumed by this. It stays an exact
     * sphere with its own parameters and placement, so Back to Construction
     * shows a sphere and Resume Sculpt returns to this same frozen mesh, both
     * for the ordinary reasons and not because of anything this method does.
     *
     * <p>On any refusal the product is left in Construction, unchanged, and the
     * status line says so. It is deliberately not retried and not repaired: a
     * refusal here means native code declined a shape it validated, and hiding
     * that behind a fallback would make the one honest signal disappear.
     */
    @Override
    public void onSculptStartChosen() {
        uiState.recordStartChoice();
        showStartChooser(false);

        final double[] primitive = new double[NativeViewport.PRIMITIVE_STATE_SIZE];
        NativeViewport.constructionPrimitive(primitive);
        final int applied = NativeViewport.applyConstructionSphere(
                primitive[NativeViewport.PRIMITIVE_SPHERE_DIAMETER]);
        // UNCHANGED is a success: it means the body was already exactly this
        // sphere, which is a perfectly good thing to sculpt.
        final boolean shaped = applied == NativeViewport.APPLY_APPLIED
                || applied == NativeViewport.APPLY_UNCHANGED;
        if (!shaped || NativeViewport.freezeToSculpt() != NativeViewport.SCULPT_OK) {
            syncFromNative();
            showStatus(getContext().getString(R.string.status_sculpt_start_failed),
                    R.attr.fsTextError);
            return;
        }
        finishEditing();
        syncFromNative();
        showStatus(getContext().getString(R.string.status_started_sculpt),
                R.attr.fsTextSuccess);
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
        pushReducedMotion();
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

        // The scene list is refreshed HERE, in every mode, because the workspace
        // owns it and it is on screen in modes the Construction editors are not.
        // It used to be refreshed as a side effect of the shape editor's own
        // re-read, which meant a docked column kept showing whatever the scene
        // looked like on the way into Sculpt. Body switching is refused while
        // sculpting, so this changes nothing about which body is active; it only
        // stops a visible list lying about the scene.
        objectsSection.refreshFromNative();

        // Whether the scene gets a column of its own depends on the MODE as well
        // as on the window — see applyObjectsPlacement — so a mode change has to
        // re-ask. The window's half of the answer is remembered from the last
        // adaptive pass, so this costs one comparison whenever nothing changed.
        applyObjectsPlacement();

        // Creation is refused below JNI while sculpting, so it is not OFFERED
        // while sculpting. Both hosts are told, from the one refresh, in every
        // mode — so a window with a column and a window with a capsule cannot
        // disagree about whether a body can be created.
        objectsCapsule.showCreationAvailable(!sculpting);
        objectsSection.showCreationAvailable(!sculpting);
        if (sculpting) {
            // The palette is anchored to a control that has just gone. Left open
            // it would stand on the model attached to nothing, and choosing a
            // tile from it would reach exactly the refusal this removes.
            setAddPrimitiveOpen(false, null);
        }

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
        objectsCapsule.refreshFromNative();
        refreshHistoryControls(sculpting);
        showActiveInspectorBody(sculpting);
        showPrecisionToggle(sculpting);
        showDefaultStatus(sculpting);
    }

    /**
     * Makes the two history controls say what native code actually reports.
     *
     * <p>Enabled state is read from native {@code canUndo}/{@code canRedo} on
     * every refresh and is never derived from anything this layer remembers.
     * There is no Java depth counter to disagree with the model: a control is
     * live exactly when a step exists, and the model changing is the feedback —
     * there is deliberately no confirmation message, no animation and nothing
     * that could stutter under a repeated tap.
     *
     * <p>In Sculpt the pair is <b>withdrawn</b>, not disabled. Sculpt has no
     * undo, and a greyed Undo sitting beside a stroke the user just made would
     * read as "your stroke can be taken back, just not yet" — which is a
     * different and worse lie than the control simply not being there. Keeping
     * them for shell consistency would also mean the one place in the product
     * where a permanently inert control stands on the model, which is the
     * pattern the Export chip is allowed as the single approved exception to.
     */
    private void refreshHistoryControls(boolean sculpting) {
        historyGroup.setVisibility(sculpting ? GONE : VISIBLE);
        if (sculpting) {
            return;
        }
        undoAction.setEnabled(NativeViewport.constructionUndoAvailable());
        redoAction.setEnabled(NativeViewport.constructionRedoAvailable());
    }

    /**
     * Steps the Construction history one entry in the asked-for direction.
     *
     * <p>The control's enabled state already answers whether there is a step, so
     * the ordinary outcome is silent: the model changes, the exact-value editors
     * re-read, and nothing is written to the status line. Only a refusal —
     * which the guard below JNI can still produce even though the controls are
     * withdrawn in Sculpt — says anything, because a control that did nothing
     * and said nothing would be the defect this reports.
     */
    private void onHistoryStepRequested(boolean forward) {
        final int status = forward ? NativeViewport.constructionRedo()
                : NativeViewport.constructionUndo();
        finishEditing();
        onNativeStateChanged();
        if (status == NativeViewport.HISTORY_REFUSED_IN_SCULPT) {
            showStatus(getContext().getString(R.string.status_history_refused_in_sculpt),
                    R.attr.fsTextError);
        }
    }

    /**
     * Writes what STANDS in the status line — which is usually nothing.
     *
     * <p>The line used to be written on every refresh with something ambient:
     * "Showing the current box in m." in Construction, and the gesture rule in
     * Sculpt. Both were true and neither was news, and because nothing ever
     * cleared the line, the last verdict — a body selected, a shape applied —
     * simply sat there until the next one replaced it. A line that always says
     * something is a line nobody reads, and it spends a capsule at the top of
     * the model to do it.
     *
     * <p>So the resting line says nothing, and the capsule is not drawn at all.
     * What the ambient messages said is said better elsewhere and permanently:
     * the precision surface's own title names the body and the mode, the unit
     * chips name the unit, and the gesture rule is written as a transient when
     * the user enters Sculpt or changes tool — at the moment it is news.
     *
     * <p><b>One exception, and it is a state rather than a verdict.</b> A stale
     * Construction Source is a standing fault: it is still true after any
     * message that covers it, and it stays true until the user acts. It lives in
     * the Sculpt context surface beside the action that resolves it, and that
     * surface no longer opens by itself, so it is also written here — where it
     * re-asserts itself on every refresh.
     */
    private void showDefaultStatus(boolean sculpting) {
        if (sculpting && nativeSculpt[NativeViewport.SCULPT_SOURCE_STALE] != 0.0) {
            toolbar.showStandingStatus(getContext().getString(R.string.stale_source_warning),
                    R.attr.fsTextMeasure);
            return;
        }
        toolbar.showStandingStatus("", R.attr.fsTextSecondary);
    }

    /**
     * The gesture rule for the tool now held, as a transient.
     *
     * <p>Written when entering Sculpt and when changing tool, which is when it
     * is news. It used to be the ambient status line, permanently — which on a
     * short landscape window also meant a sentence ellipsised in the middle of
     * the clause carrying the rule, standing there for the whole session.
     */
    private void showSculptGestureHint() {
        final int tool = (int) nativeSculpt[NativeViewport.SCULPT_TOOL];
        final int index = (tool >= 0 && tool < SCULPT_TOOL_HINTS.length)
                ? tool : NativeViewport.TOOL_GRAB;
        final Context context = getContext();
        showStatus(context.getString(R.string.sculpt_gesture_rule,
                context.getString(SCULPT_TOOL_HINTS[index])), R.attr.fsTextSecondary);
    }

    private void buildRailFor(boolean sculpting) {
        if (railShowsSculptTools == sculpting && toolRail.getChildCount() > 0) {
            return;
        }
        railShowsSculptTools = sculpting;
        final Context context = getContext();
        if (sculpting) {
            toolRail.setEntries(new ToolRailView.Entry[]{
                    new ToolRailView.Entry(R.id.tool_rail_grab, R.drawable.ic_tool_grab,
                            context.getString(R.string.tool_grab),
                            NativeViewport.TOOL_GRAB),
                    new ToolRailView.Entry(R.id.tool_rail_clay, R.drawable.ic_tool_clay,
                            context.getString(R.string.tool_clay),
                            NativeViewport.TOOL_CLAY),
                    new ToolRailView.Entry(R.id.tool_rail_smooth, R.drawable.ic_tool_smooth,
                            context.getString(R.string.tool_smooth),
                            NativeViewport.TOOL_SMOOTH),
                    new ToolRailView.Entry(R.id.tool_rail_inflate, R.drawable.ic_tool_inflate,
                            context.getString(R.string.tool_inflate),
                            NativeViewport.TOOL_INFLATE),
            });
        } else {
            // Two entries, and both of them work.
            //
            // Sketch and Extrude used to be drawn here, inert. The argument was
            // that the shell's shape should not change when they arrive — but
            // the cost was half of the one control the user reaches for most
            // spent on features the product does not have, on the smallest
            // window, next to the two that do. A rail is a set of tools; an
            // entry that looks like a tool and does nothing is worse than an
            // absent one, and nothing about this rail's structure has to change
            // to take a third working entry later.
            //
            // "Transform" rather than "Place" is the vocabulary Stage 020's
            // direct handles will join. It is not a claim that they exist: what
            // this entry opens today is exact numeric and its own title says
            // so — "Exact Transform — Body #1".
            toolRail.setEntries(new ToolRailView.Entry[]{
                    new ToolRailView.Entry(R.id.tool_rail_shape, R.drawable.ic_tool_shape,
                            context.getString(R.string.tool_shape),
                            EditorUiState.CONSTRUCTION_TOOL_SHAPE),
                    new ToolRailView.Entry(R.id.tool_rail_place, R.drawable.ic_tool_place,
                            context.getString(R.string.tool_transform),
                            EditorUiState.CONSTRUCTION_TOOL_TRANSFORM),
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
    /**
     * Puts the right body in the inspector, and names the object it edits.
     *
     * <p>The Construction titles carry the active body — "Shape — Body #2" —
     * because the scene list no longer sits inside this panel and the panel must
     * still say <b>which</b> object its numbers describe. The name is read from
     * native scene state at the moment the title is written, never remembered
     * here. Sculpt is titled by mode alone: it edits the one Frozen Sculpt Mesh
     * and body switching is refused while it is open, so naming a body there
     * would imply a choice that does not exist.
     */
    private void showActiveInspectorBody(boolean sculpting) {
        final Context context = getContext();
        if (sculpting) {
            inspector.setBody(sculptContext, context.getString(R.string.inspector_sculpt_title));
            return;
        }
        final String body = context.getString(R.string.body_label,
                NativeViewport.sceneActiveBodyId());
        if (uiState.constructionTool() == EditorUiState.CONSTRUCTION_TOOL_TRANSFORM) {
            inspector.setBody(placementEditor,
                    context.getString(R.string.inspector_place_title_for_body, body));
        } else {
            inspector.setBody(shapeEditor,
                    context.getString(R.string.inspector_shape_title_for_body, body));
        }
    }

    /**
     * Puts the precision toggle and its surface in step with the held tool.
     *
     * <p>The toggle names what it will open, so a user never has to press it to
     * find out; the surface itself is opened or closed from what the user last
     * decided for this mode, which starts closed.
     */
    private void showPrecisionToggle(boolean sculpting) {
        final Context context = getContext();
        final String opens = context.getString(precisionSurfaceName(sculpting));
        final boolean open = uiState.precisionOpen(sculpting);
        precisionToggle.setContentDescription(context.getString(
                open ? R.string.precision_close : R.string.precision_open, opens));
        EditorControlStyles.setIconButtonActive(precisionToggle, open);
        applyPrecisionOpen(open);
    }

    /** What the precision toggle opens, given the mode and the held entry. */
    private int precisionSurfaceName(boolean sculpting) {
        if (sculpting) {
            return R.string.precision_sculpt;
        }
        return uiState.constructionTool() == EditorUiState.CONSTRUCTION_TOOL_TRANSFORM
                ? R.string.precision_transform : R.string.precision_shape;
    }

    /**
     * The user asked for, or dismissed, the exact values behind the held tool.
     *
     * <p>Recorded per mode so it survives a refresh and a rotation, and applied
     * at once. It makes <b>no native call</b>: opening a panel of numbers reads
     * state that is already there, publishes nothing and uploads nothing.
     */
    private void onPrecisionToggleRequested() {
        setPrecisionOpen(!inspector.isOpen());
    }

    @Override
    public void onPrecisionCloseRequested() {
        setPrecisionOpen(false);
    }

    private void setPrecisionOpen(boolean open) {
        final boolean sculpting = isSculpting();
        uiState.setPrecisionOpen(sculpting, open);
        if (open) {
            // The precision surface is a context surface like any other, so it
            // takes the screen from whatever else was standing on the model.
            setObjectsPanelOpen(false);
            setAddPrimitiveOpen(false, null);
            displayPopover.setOpen(false);
            toolbar.showDisplaySettingsOpen(false);
        } else {
            // Typing is over; the keyboard and the focus belong back on the
            // model rather than on a field that has just left the window.
            finishEditing();
        }
        showPrecisionToggle(sculpting);
    }

    /**
     * Opens or closes the panel, growing it out of the toggle that owns it.
     *
     * <p>The anchor runs only for a bottom sheet: the two side placements are
     * laid out in the chrome row beside the model rather than in the overlay,
     * so their position is the layout's and not this method's.
     */
    private void applyPrecisionOpen(boolean open) {
        if (open) {
            // A bottom sheet unfolds UP out of the toggle beside the rail; a side
            // placement hangs from the top of the chrome row, so it unfolds DOWN
            // from its own top corner. Stated for both, because "whatever it was
            // last set to" is not an anchor — a rotation from portrait into a
            // side placement would otherwise leave the panel growing from a
            // bottom edge it no longer has.
            inspector.setGrowsUpward(inspectorPlacement
                    == WorkspaceLayoutMode.InspectorPlacement.BOTTOM_SHEET);
        }
        inspector.setOpen(open);
    }

    /**
     * Tells the viewport whether to reduce motion.
     *
     * <p>Pushed on every refresh, including every resume, because the setting
     * can be changed while ForgeShape is in the background — which is exactly
     * how a user turns animation off. Reading the Android setting and deciding
     * what it means happens HERE; what crosses JNI is one bool, so the geometry
     * domain never learns that an Android setting exists.
     */
    private void pushReducedMotion() {
        NativeViewport.setReducedMotion(!ChromeMotion.animationsEnabled(getContext()));
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
                    getContext().getString(SCULPT_TOOL_HINTS[index])), R.attr.fsTextSecondary);
            finishEditing();
            return;
        }
        // In Construction the rail chooses which exact-value editor is on
        // screen. Native code has no such concept, makes no call here, and
        // nothing about the object changes.
        uiState.setConstructionTool(key);
        toolRail.showActive(uiState.constructionTool());
        showActiveInspectorBody(false);
        // The toggle belongs to the entry above it, so it re-names itself with
        // the entry. Whether the surface is OPEN is unchanged: switching
        // context while the numbers are on screen swaps the body rather than
        // dismissing the panel, and switching while it is closed leaves it
        // closed.
        showPrecisionToggle(false);
        // Nothing is written for this, and that is the point. Switching rail
        // entry is not an event to report: the rail draws which entry is held,
        // the toggle under it names what it will open, and the surface it opens
        // titles itself "Exact Shape — Body #1". "Shape and placement — edit
        // exact values, then Apply." was a third copy of the same fact, and
        // because it was written on every switch it stood over the model
        // whenever a user was doing exactly what it described.
    }

    // The brush controls report nothing upward, deliberately.
    //
    // Dragging Radius used to write "Brush: radius 120 px, strength 0.45." into
    // the status line, so the same two numbers stood on screen twice — once
    // beside the finger moving them and once in a capsule at the top of the
    // window, where nobody adjusting a brush is looking. The status copy then
    // outlived the drag by the rest of the session, which is how a resting
    // Sculpt screenshot came to be captioned with the last slider position.
    // A value being dragged belongs beside the control dragging it, and it is
    // there. See BrushEdgeControlsView.

    /**
     * Whether chrome may spend time on a transition.
     *
     * <p>Pushed down rather than queried, because the surfaces that animate
     * decide at the moment of the act and the act can arrive on a second finger
     * while the first one is mid-stroke.
     *
     * <p>Pushed to <b>every</b> anchored surface, not only the precision one.
     * All four can be opened while a finger is on the model — the Objects
     * capsule sits low, exactly where a stroke begins — and a surface that
     * animated during a stroke would be competing with pointer samples for the
     * main thread.
     */
    private void setChromeMotionAllowed(boolean allowed) {
        chromeMotionAllowed = allowed;
        inspector.setMotionAllowed(allowed);
        objectsPopover.setMotionAllowed(allowed);
        addPrimitivePalette.setMotionAllowed(allowed);
        displayPopover.setMotionAllowed(allowed);
    }

    // -----------------------------------------------------------------------
    // Global actions
    // -----------------------------------------------------------------------

    /**
     * <b>Start Sculpting.</b> Copies the object's current Construction mesh into
     * a sculpt mesh and enters Sculpt Mode.
     *
     * <p>Unguarded on purpose. This control is on screen only while <b>no</b>
     * sculpt mesh exists, so it can discard nothing: there is no sculpt work to
     * lose. The guarded path is Reset Sculpt from Shape in the Sculpt context
     * surface, which is the one that replaces an edited mesh.
     *
     * <p>It changes no dimension, no primitive kind and no placement — the
     * Construction Source is only read, and it is still here, unchanged, when
     * Sculpt Mode is left. The wording changed at UI-R4B and none of that did:
     * the method still calls {@code freezeToSculpt()}, which is still what the
     * domain calls the operation, and it is still reversible in both directions.
     */
    @Override
    public void onFreezeToSculpt() {
        if (NativeViewport.freezeToSculpt() != NativeViewport.SCULPT_OK) {
            showStatus(getContext().getString(R.string.status_sculpt_prepare_failed),
                    R.attr.fsTextError);
            return;
        }
        finishEditing();
        syncFromNative();
        showStatus(getContext().getString(R.string.status_now_sculpting,
                shapeEditor.describeNativeKind()), R.attr.fsTextSuccess);
    }

    /**
     * Returns to the sculpt mesh exactly as it was left.
     *
     * <p>Never confirmed, because it destroys nothing: nothing that has been
     * sculpted is lost by having looked at the Construction Source. Guarding it
     * would train the user to dismiss the guard that matters.
     */
    @Override
    public void onResumeSculpt() {
        if (NativeViewport.enterSculptMode() != NativeViewport.SCULPT_OK) {
            showStatus(getContext().getString(R.string.status_no_sculpt_mesh),
                    R.attr.fsTextError);
            return;
        }
        finishEditing();
        syncFromNative();
        // Says what the finger will do now, once, rather than standing in the
        // status line for the rest of the session. A stale Construction Source
        // outranks it and syncFromNative has already written that as the
        // STANDING message, so this must not overwrite one.
        if (nativeSculpt[NativeViewport.SCULPT_SOURCE_STALE] == 0.0) {
            showSculptGestureHint();
        }
    }

    /**
     * Back to the Construction Source, which sculpting never wrote.
     *
     * <p>The wording elsewhere changed at UI-R4B and what this does did not. The
     * exact primitive, its parameters and its placement are exactly what they
     * were before Start Sculpting — no sculpt edit has ever been allowed to
     * reach them — and Resume Sculpt returns to the same mesh with the same
     * revision, the same counts and the same stroke history.
     */
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

    /**
     * The Objects capsule's expand control.
     *
     * <p>Toggles the scene panel, grown out of the capsule itself. Nothing
     * about the scene, the active body or any geometry changes here: opening a
     * list is presentation, and the rows themselves are still the only thing
     * that asks native code to select.
     */
    @Override
    public void onObjectsRequested() {
        setObjectsPanelOpen(!objectsPopover.isOpen());
    }

    @Override
    public void onDisplaySettingsRequested() {
        final boolean opening = !displayPopover.isOpen();
        if (opening) {
            // One context surface at a time; opening one closes the others
            // rather than stacking them.
            setObjectsPanelOpen(false);
            setAddPrimitiveOpen(false, null);
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

    @Override
    public void onProjectionModeRequested(int mode) {
        // Camera state, so this goes to the camera rather than the display
        // store — but from the user's side it is the same kind of act as the
        // chips above it, and the popover stays open for the same reason:
        // judging Perspective against Orthographic means switching repeatedly.
        //
        // Nothing else is refreshed. A projection change publishes no mesh and
        // mints no revision, so no inspector value and no status line can have
        // become stale because of it.
        NativeViewport.setProjectionMode(mode);
        refreshDisplaySettings();
    }

    /**
     * Switches the whole workspace's appearance.
     *
     * <p>The one control here that is <b>not</b> answered by native code, and
     * the only one that does not repaint in place: applying a theme means
     * re-resolving every themed resource, which the Activity does by recreating
     * itself. So there is nothing to refresh afterwards — this view is about to
     * be replaced by one built in the new theme.
     *
     * <p>It changes no Construction parameter, no transform, no
     * {@code ObjectId}, no revision, nothing about what is pickable and nothing
     * about what is selected: all of that is process-scoped native state that
     * outlives the Activity, which is exactly why recreating it is safe.
     */
    @Override
    public void onAppThemeRequested(AppTheme theme) {
        if (theme == uiState.appTheme()) {
            // Already wearing it. Repaint the chips rather than recreating, so a
            // tap on the current appearance is inert instead of a visible flash.
            refreshDisplaySettings();
            return;
        }
        final Context context = getContext();
        if (context instanceof ForgeShapeActivity) {
            ((ForgeShapeActivity) context).requestTheme(theme);
        }
    }

    /**
     * Shows or hides the world reference grid.
     *
     * <p>The same shape of act as the shading chips above it, and the popover
     * stays open for the same reason: deciding whether a floor helps the body
     * you are placing means switching it back and forth.
     *
     * <p>Nothing else is refreshed, because nothing else can have gone stale.
     * The grid is a viewport reference, not geometry: it has no
     * {@code ObjectId}, is not in the scene, is not pickable, and toggling it
     * mints no revision, rebuilds no render mesh and uploads nothing. Calling
     * {@code syncFromNative()} here would throw away a half-typed dimension for
     * a change that did not touch a single value in the fields.
     */
    @Override
    public void onGridVisibleRequested(boolean visible) {
        final boolean inEffect = NativeViewport.setGridVisible(visible);
        refreshDisplaySettings();
        showStatus(getContext().getString(
                inEffect ? R.string.status_grid_on : R.string.status_grid_off),
                R.attr.fsTextSecondary);
    }

    /** Repaints the popover from native truth, so a refused request shows. */
    private void refreshDisplaySettings() {
        displayPopover.showSettings(NativeViewport.shadingModel(),
                NativeViewport.surfaceShading(), NativeViewport.projectionMode(),
                uiState.appTheme(), NativeViewport.gridVisible());
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
    public void showStatus(CharSequence message, int colorAttr) {
        toolbar.showStatus(message, colorAttr);
    }

    @Override
    public void onNativeStateChanged() {
        // Recorded HERE, in the one place every surface re-reads, rather than
        // in the viewport gesture listener that consults it. Tracking it only
        // there left it stale whenever the active body changed by some other
        // route — Add Body, or an Objects row — and a later viewport pick that
        // happened to land back on the stale value then compared equal and
        // skipped the refresh, leaving the Inspector showing another body's
        // numbers. Updating it wherever the chrome actually re-reads makes the
        // comparison mean what it says: "has the active body changed since the
        // last time these surfaces were refreshed?"
        lastKnownActiveBodyId = NativeViewport.sceneActiveBodyId();
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
                allConverted ? R.attr.fsTextSecondary : R.attr.fsTextError);
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

    ConstructionShapeEditorView shapeEditor() {
        return shapeEditor;
    }

    /**
     * The Objects section, so a test can select a body by its ObjectId.
     *
     * <p>The same instance in every layout mode — that is the point. A test
     * asserts what it does, not where it currently hangs.
     */
    ObjectsSectionView objectsSection() {
        return objectsSection;
    }

    /** The expanded window's Objects column, so a test can measure it. */
    ScrollView objectsDock() {
        return objectsDock;
    }

    /** Whether Objects currently has a surface of its own. */
    boolean objectsDocked() {
        return objectsDock.getVisibility() == VISIBLE
                && objectsSection.getParent() == objectsDock;
    }

    /** The scene panel, so a test can open it and measure what it costs. */
    ObjectsPopoverView objectsPopover() {
        return objectsPopover;
    }

    /**
     * The resting scene control.
     *
     * <p>The same instance in every mode and every window — that is the point.
     * A test asserts what it exposes, not where it currently hangs.
     */
    ObjectsCapsuleView objectsCapsule() {
        return objectsCapsule;
    }

    /** The one creation surface, so a test can enumerate what it offers. */
    AddPrimitivePaletteView addPrimitivePalette() {
        return addPrimitivePalette;
    }

    /** The Display popover, so a test can name the fourth anchored surface. */
    DisplaySettingsPopoverView displayPopover() {
        return displayPopover;
    }

    /**
     * Every surface that grows out of a control, in one place.
     *
     * <p>So a case can assert the shared motion contract over the SET rather
     * than over a list it maintains by hand — which is how the fourth surface
     * came to be the only one with a correct first-open pivot: nothing was
     * measuring them together.
     */
    AnchoredSurfaceView[] anchoredSurfaces() {
        return new AnchoredSurfaceView[]{
                objectsPopover, addPrimitivePalette, inspector, displayPopover};
    }

    /** The Tool Rail, so a test can read which entry it says is held. */
    ToolRailView toolRail() {
        return toolRail;
    }

    /** The direct brush controls, so a test can read the values beside them. */
    BrushEdgeControlsView brushControls() {
        return brushControls;
    }

    /** The rail's precision toggle, so a test can open the exact values the way
     *  a user does rather than by calling into the workspace. */
    ImageView precisionToggle() {
        return precisionToggle;
    }

    /** The capsule the precision toggle sits in, which is what actually stands
     *  on the model and therefore what a chrome measurement must use. */
    View precisionGroup() {
        return precisionGroup;
    }

    /** The trailing tool cluster: rail plus precision toggle. */
    View railColumn() {
        return railColumn;
    }

    /** The workspace's bottom edge, so a test can prove what it costs. */
    View bottomRow() {
        return bottomRow;
    }

    /** The capsule Undo and Redo sit in, which is what actually stands on the
     *  model — the controls themselves are inside it. */
    View historyGroup() {
        return historyGroup;
    }

    /** Undo, so a test drives the control a user would press. */
    ImageView undoAction() {
        return undoAction;
    }

    /** Redo, the same way. */
    ImageView redoAction() {
        return redoAction;
    }

    /** The Global Toolbar, so a test can reach a global control by id. */
    GlobalToolbarView globalToolbar() {
        return toolbar;
    }

    /** Whether the Tool Rail is currently drawn flush rather than floating. */
    boolean railDocked() {
        return appliedRailDocked != null && appliedRailDocked;
    }

    /**
     * Puts the start question back and shows it, as a fresh process would.
     *
     * <p>The instrumentation runs every case in one process, so without this
     * only the first test could ever see an unanswered chooser. It changes no
     * native state: the scene, the mode and the body are exactly what they
     * were, and only whether the question is drawn is different.
     */
    void showStartChooserAsFirstLaunch() {
        uiState.clearStartChoice();
        showStartChooser(true);
    }

    /**
     * The scroll container the Tool Rail lives in.
     *
     * <p>Needed by verification because the rail's tap-versus-scroll rule is a
     * negotiation BETWEEN the entry and this container, so a test that
     * dispatched only to the rail would never exercise the interception the
     * rule exists to settle.
     */
    ScrollView toolRailScroll() {
        return toolRailScroll;
    }

    /** Answers the start question the way a test that is not about it needs. */
    void dismissStartChooserForConstruction() {
        uiState.recordStartChoice();
        showStartChooser(false);
    }

    /** The chrome rectangles, in this view's coordinates, that stand between
     *  the user and the model. Empty when chrome is hidden. */
    Rect[] chromeRects() {
        if (uiState.chromeHidden()) {
            return new Rect[0];
        }
        // The toolbar CONTAINER is not in this list, and that is a fact about
        // the composition rather than a convenience: it is transparent and
        // draws nothing, so counting its full-width rect as occlusion would
        // report a bar the user cannot see. What actually stands on the model up
        // there is its two control capsules and the status capsule, which is
        // exactly what the toolbar reports.
        // The bottom ROW is not in this list for the same reason, and the two
        // capsules inside it are: the row spans the window and paints nothing,
        // and counting it would report the bottom bar the composition
        // deliberately does not have.
        final View[] surfaces = {brushControls, toolRailScroll, precisionGroup,
                objectsCapsule, historyGroup, inspector, objectsDock};
        final View[] top = toolbar.occludingSurfaces();
        final View[] all = new View[surfaces.length + top.length];
        System.arraycopy(surfaces, 0, all, 0, surfaces.length);
        System.arraycopy(top, 0, all, surfaces.length, top.length);
        return chromeRectsFor(all);
    }

    private Rect[] chromeRectsFor(View[] surfaces) {
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
