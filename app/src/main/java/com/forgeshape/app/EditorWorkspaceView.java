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
import android.view.WindowInsetsAnimation;
import android.view.inputmethod.InputMethodManager;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;

import java.util.List;

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
        WorkspaceTrailingHostView.Callbacks, PropertyInspectorView.OnPrecisionSurfaceClosed,
        DisplaySettingsPopoverView.OnDisplaySettingChanged,
        ProjectActionsPopoverView.OnProjectAction,
        RecoveryPromptView.OnRecoveryChoice,
        ObjectsCapsuleView.OnObjectsCapsuleAction,
        AddPrimitivePaletteView.OnPrimitiveChosen,
        StartChooserView.OnStartFlowChosen,
        SketchEditorView.OnSketchAction,
        AnchoredSurfaceView.OnOpenStateChanged {

    private static final int[] SCULPT_TOOL_HINTS = {
            R.string.hint_grab, R.string.hint_clay, R.string.hint_smooth, R.string.hint_inflate
    };
    /** The five sketch tools by name and by gesture rule, indexed by
     *  {@code SKETCH_TOOL_*}. */
    private static final int[] SKETCH_TOOL_NAMES = {
            R.string.tool_select, R.string.tool_line, R.string.tool_polyline,
            R.string.tool_rectangle, R.string.tool_circle
    };
    private static final int[] SKETCH_TOOL_HINTS = {
            R.string.hint_select, R.string.hint_line, R.string.hint_polyline,
            R.string.hint_rectangle, R.string.hint_circle
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
    /** Tracks sketch mode so a sketch STARTED in native (by the spatial
     *  chooser's confirm tap, not by a Java call) still rebuilds the chrome. */
    private boolean lastKnownSketching = false;
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
    /** Presentation-only owner of the Tool Rail, Exact trigger and selectors. */
    private final WorkspaceTrailingHostView trailingHost;

    private final BrushEdgeControlsView brushControls;
    private final PropertyInspectorView inspector;
    private final ImageView restoreChip;
    private final DisplaySettingsPopoverView displayPopover;
    private final ProjectActionsPopoverView projectPopover;
    private final StartChooserView startChooser;
    private final RecoveryPromptView recoveryPrompt;

    /**
     * Who decides when the project is checkpointed. Owned here because this is
     * the one view every semantic edit passes through; released with the view.
     */
    private final AutosaveController autosave;

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
    /** The sketch's precision surface, and a CAD Body's (CAD-R0-A1A2). */
    private final SketchEditorView sketchEditor;
    private final CadFeatureEditorView cadEditor;
    /** Reused across reads; native fills this with the sketch session's state. */
    private final double[] nativeSketch = new double[NativeViewport.SKETCH_STATE_SIZE];
    /** The last sketch refusal the status line reported, so a gesture that
     *  repeats the same refusal does not repeat the sentence. */
    private int lastReportedSketchStatus = NativeViewport.CAD_OK;

    /** Reused across reads; native fills it with the authoritative state. */
    private final double[] nativeSculpt = new double[NativeViewport.SCULPT_STATE_SIZE];

    /** Scratch for the gizmo read-back. Reused rather than allocated per
     *  refresh: this is read on every sync and on every settled gesture. */
    private final double[] nativeGizmo = new double[NativeViewport.GIZMO_STATE_SIZE];

    /**
     * How many gizmo drags had committed a step when a gesture last settled.
     *
     * <p>NOT a history depth and not a mirror of one: it is a monotone counter
     * used for exactly one comparison — did the model move under the finger, or
     * did the camera merely orbit. Without it every viewport gesture in
     * Transform would re-read the exact-value editors and discard a half-typed
     * draft.
     */
    private long lastKnownGizmoCommits;

    /**
     * The active body's sculpt undo depth when a gesture last settled.
     *
     * <p>The same idea as {@link #lastKnownGizmoCommits} and for the same
     * reason: a sculpt stroke never passes through Java, so the settled-gesture
     * edge is the only moment this layer can learn one happened. Comparing the
     * depth answers "did a stroke actually commit an entry", where re-reading
     * unconditionally would rewrite the brush controls after every orbit.
     *
     * <p>Emphatically not a mirror of the history: it is one number used for one
     * comparison, it is never read to decide what Undo does, and the enabled
     * state still comes from native on every refresh.
     */
    private int lastKnownSculptUndoDepth;

    /**
     * How many oversized strokes had been reported to the user, so the same one
     * is not announced twice. See {@link #reportUnretainedStrokes}.
     */
    private long unretainedStrokesReported;

    private WorkspaceLayoutMode layoutMode = WorkspaceLayoutMode.COMPACT;
    private WorkspaceLayoutMode.InspectorPlacement inspectorPlacement =
            WorkspaceLayoutMode.InspectorPlacement.BOTTOM_SHEET;

    /** The window the current arrangement was computed for, so the decision
     *  runs once per size rather than once per measure pass. */
    private int appliedWidthPx;
    private int appliedHeightPx;

    /** Whether this window uses compact-height rail entries. */
    private boolean shortWindow;

    /** Whether the soft keyboard is consuming the lower window. Root-owned
     *  inset state never changes the right host's vertical layout vocabulary. */
    private boolean keyboardVisible;

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
                            // Unconditionally, and before any of the questions
                            // below. A sculpt stroke never passes through Java
                            // — it is resolved entirely in native code from the
                            // touch samples — so this gesture edge is the only
                            // moment the Android layer ever learns that a stroke
                            // may have happened. Noting is cheap and the
                            // controller decides whether anything actually
                            // changed; not noting would leave a whole stroke
                            // unprotected until some unrelated edit came along.
                            noteProjectMaybeDirty();
                            // A sketch gesture places, selects or refuses; the
                            // only chrome with something new to say is the
                            // sketch's own precision surface and the status
                            // line. Nothing about the scene moved.
                            // A sketch may have STARTED in native since the last
                            // gesture -- the spatial chooser's confirm tap begins
                            // one with no Java call -- so a mode transition here
                            // must rebuild the chrome, not just refresh the
                            // sketch surface.
                            final boolean sketchingNow = isSketching();
                            if (sketchingNow != lastKnownSketching) {
                                lastKnownSketching = sketchingNow;
                                onNativeStateChanged();
                                return;
                            }
                            if (sketchingNow) {
                                onSketchGestureSettled();
                                return;
                            }
                            final long active = NativeViewport.sceneActiveBodyId();
                            if (active != lastKnownActiveBodyId) {
                                lastKnownActiveBodyId = active;
                                onNativeStateChanged();
                                return;
                            }
                            // A gizmo drag that committed a step moved the
                            // authoritative placement, so the exact values and
                            // the history controls both have something new to
                            // say. An orbit did not, and re-reading for one
                            // would throw away a half-typed draft — which is
                            // why this asks whether a drag COMMITTED rather
                            // than whether a gesture happened.
                            // A completed sculpt stroke is the same kind of
                            // event for the OTHER history: it made an Undo
                            // available where there was none. Only the two
                            // history controls have anything new to say, so
                            // this refreshes them rather than calling
                            // syncFromNative — a full re-read after every
                            // stroke would rewrite the brush controls under a
                            // user who is still sculpting with them.
                            final int sculptDepth = NativeViewport.sculptUndoDepth();
                            if (sculptDepth != lastKnownSculptUndoDepth) {
                                lastKnownSculptUndoDepth = sculptDepth;
                                refreshHistoryControls();
                                reportUnretainedStrokes();
                            }

                            NativeViewport.gizmoState(nativeGizmo);
                            final long commits =
                                    (long) nativeGizmo[NativeViewport.GIZMO_COMMITTED_DRAGS];
                            if (commits != lastKnownGizmoCommits) {
                                lastKnownGizmoCommits = commits;
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
        // The row is spatial, not typographic. Baseline alignment makes a tall
        // right host move when the IME changes the row's available height.
        middleRow.setBaselineAligned(false);
        middleRow.setGravity(Gravity.TOP);
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
        // Sculpt uses the same stable top anchor as the trailing tool cluster.
        // Changing the brush track height must not re-centre the whole panel or
        // move an unrelated control under the user's hand.
        brushParams.gravity = Gravity.TOP;
        brushParams.leftMargin = EditorControlStyles.dimen(context, R.dimen.brush_gap);
        brushParams.topMargin = EditorControlStyles.dimen(context, R.dimen.row_gap);
        middleRow.addView(brushControls, brushParams);

        // The empty middle is where the model lives. It is a weighted gap with
        // no background and no listener, so it costs the viewport nothing.
        middleRow.addView(EditorControlStyles.spacer(context));

        trailingHost = new WorkspaceTrailingHostView(context, this);

        middleRow.addView(trailingHost, trailingHost.parentLayoutParams());

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

        // The project surface is anchored the same way and for the same reason:
        // its control is in the toolbar, and the overlay is the only place a
        // panel can hang under a fixed-height row without resizing it.
        projectPopover = new ProjectActionsPopoverView(context, this);
        overlayRoot.addView(projectPopover, ProjectActionsPopoverView.anchoredParams(context,
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
        sketchEditor = new SketchEditorView(context, this, this);
        cadEditor = new CadFeatureEditorView(context, this);

        // Last into the overlay, so the question is above everything it is
        // asking about. It stands on a WORKING workspace: native state already
        // exists (the Activity starts native code before building any view),
        // the default Body is already there, and the viewport is already
        // rendering it behind the scrim. Choosing Construction therefore has
        // nothing to build — it only stops asking.
        startChooser = new StartChooserView(context, this);
        overlayRoot.addView(startChooser, new LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));

        // Above the start question, because it is asked FIRST: if there is
        // unsaved work to recover, how a new model would have begun is not yet
        // a question worth asking.
        recoveryPrompt = new RecoveryPromptView(context, this);
        recoveryPrompt.setVisibility(GONE);
        overlayRoot.addView(recoveryPrompt, new LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));

        // Every anchored surface reports its own open state to one listener, so
        // "is there something for System Back to dismiss" is answered by the
        // surfaces themselves rather than by a list of call sites that has to
        // stay complete.
        for (AnchoredSurfaceView surface : anchoredSurfaces()) {
            surface.setOnOpenStateChanged(this);
        }

        installInsetListener();
        syncFromNative();

        autosave = new AutosaveController(context);

        // The recovery question is asked ONCE PER PROCESS and before the start
        // question, and both of those are the same rule: this is the moment the
        // session begins, and it begins once. An Activity recreation — a theme
        // change, a rotation the config did not absorb — rebuilds this view, and
        // asking again there would present a second decision about a candidate
        // the user has already answered.
        final boolean asking = !uiState.recoveryResolved() && offerRecoveryIfPresent();
        showStartChooser(!asking && !uiState.startChoiceMade());
    }

    // -----------------------------------------------------------------------
    // Dismissing a context surface — what System Back means here
    // -----------------------------------------------------------------------

    /** Told when the workspace gains or loses something Back should dismiss. */
    interface OnDismissibleSurfaceChanged {
        void onDismissibleSurfaceChanged(boolean present);
    }

    /** The Activity, which owns the platform's Back registration. */
    private OnDismissibleSurfaceChanged dismissibleSurfaceListener;

    /**
     * The surface a Back press would dismiss: the one most recently opened.
     *
     * <p>Tracked rather than derived, because "topmost" is an ordering the view
     * tree does not record — the anchored surfaces all live in one overlay and
     * none of them is above another in z. Today they are mutually exclusive, so this is
     * usually the only open one; it is tracked anyway so that stops being an
     * assumption the dismissal rule silently depends on.
     */
    private AnchoredSurfaceView topmostSurface;

    void setOnDismissibleSurfaceChanged(OnDismissibleSurfaceChanged listener) {
        dismissibleSurfaceListener = listener;
        if (listener != null) {
            listener.onDismissibleSurfaceChanged(hasDismissibleSurface());
        }
    }

    @Override
    public void onSurfaceOpenStateChanged(AnchoredSurfaceView surface, boolean open) {
        if (open) {
            topmostSurface = surface;
        } else if (topmostSurface == surface) {
            topmostSurface = null;
        }
        if (dismissibleSurfaceListener != null) {
            dismissibleSurfaceListener.onDismissibleSurfaceChanged(hasDismissibleSurface());
        }
        applyPrimarySurfaceChromePolicy();
    }

    @Override
    public void onSurfacePresentationSettled(AnchoredSurfaceView surface, boolean visible) {
        // A closing bottom sheet remains layout-present for its whole exit.
        // Restore the bottom zone only after that surface is actually absent.
        applyPrimarySurfaceChromePolicy();
    }

    /**
     * The immediate static chrome policy owned by the currently open primary
     * surface.
     *
     * <p>A compact precision/details sheet owns the lower region for its full
     * entry and exit. The Objects/history row withdraws instead of being moved
     * above the sheet, and returns only after the sheet is absent. A Display
     * surface owns the upper trailing region, so the
     * transform/tool cluster is temporarily absent instead of remaining live
     * underneath it. Side inspectors have their own column and therefore leave
     * the bottom row alone.
     *
     * <p>Visibility changes are deliberately not animated in this stage. The
     * persistent groups keep their top anchors when they return.
     */
    private void applyPrimarySurfaceChromePolicy() {
        if (bottomRow == null || trailingHost == null || inspector == null
                || displayPopover == null) {
            return;
        }
        final boolean inspectorPresented = inspector.isOpen()
                || inspector.getVisibility() == VISIBLE;
        final boolean lowerRegionOwned = inspectorPresented
                && inspectorPlacement == WorkspaceLayoutMode.InspectorPlacement.BOTTOM_SHEET;
        bottomRow.setVisibility(lowerRegionOwned ? GONE : VISIBLE);
        renderTrailingHost(isSculpting());
    }

    /** Whether a Back press has a surface to close before it may leave. */
    boolean hasDismissibleSurface() {
        return topmostOpenSurface() != null;
    }

    private AnchoredSurfaceView topmostOpenSurface() {
        if (topmostSurface != null && topmostSurface.isOpen()) {
            return topmostSurface;
        }
        for (AnchoredSurfaceView surface : anchoredSurfaces()) {
            if (surface.isOpen()) {
                return surface;
            }
        }
        return null;
    }

    /**
     * Closes the topmost dismissible surface, if there is one.
     *
     * <p><b>Through the workspace's own close path, never through the surface.</b>
     * Closing the precision surface also releases the keyboard and un-lights the
     * toggle; closing the palette un-lights the capsule's plus. A Back press has
     * to mean exactly what pressing the control again means, or the two ways of
     * dismissing one surface leave the workspace in two different states.
     *
     * <p>It also goes through {@code setOpen} rather than
     * {@link AnchoredSurfaceView#closeImmediately()}, so the surface plays the
     * same exit it plays for its own control — which is the whole of the motion
     * seam this needed: the dismissal animation was already written and the most
     * common dismissal gesture simply never reached it. Reduced motion still
     * lands instantly, because that decision is inside {@code setOpen}.
     *
     * @return whether a surface was dismissed; false means Back is not ours
     */
    boolean dismissTopmostSurface() {
        // Spatial support selection is not a surface but it is the innermost
        // thing System Back should leave: cancel it before anything else, with
        // no project mutation, and put the camera back.
        if (NativeViewport.supportChooserActive()) {
            NativeViewport.supportChooserCancel();
            onNativeStateChanged();
            showStatus("", R.attr.fsTextSecondary);
            return true;
        }
        final AnchoredSurfaceView surface = topmostOpenSurface();
        if (surface == null) {
            return false;
        }
        if (surface == inspector) {
            setPrecisionOpen(false);
        } else if (surface == addPrimitivePalette) {
            setAddPrimitiveOpen(false, null);
        } else if (surface == objectsPopover) {
            setObjectsPanelOpen(false);
        } else if (surface == displayPopover) {
            displayPopover.setOpen(false);
            toolbar.showDisplaySettingsOpen(false);
        } else if (surface == projectPopover) {
            projectPopover.setOpen(false);
            toolbar.showProjectActionsOpen(false);
        } else {
            surface.setOpen(false);
        }
        return true;
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
                applyChromeInsets(insets);
                // Returned unconsumed: this view has decided what chrome does
                // about them, and the viewport deliberately ignores them.
                return insets;
            }
        });
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            setWindowInsetsAnimationCallback(new WindowInsetsAnimation.Callback(
                    WindowInsetsAnimation.Callback.DISPATCH_MODE_CONTINUE_ON_SUBTREE) {
                @Override
                public WindowInsets onProgress(WindowInsets insets,
                                               List<WindowInsetsAnimation> runningAnimations) {
                    // onApplyWindowInsets may run only at the start of an IME
                    // transition. Follow every animated frame so bottom-sheet
                    // chrome never remains at that first, partial inset.
                    applyChromeInsets(insets);
                    return insets;
                }
            });
        }
    }

    private void applyChromeInsets(WindowInsets insets) {
        // During the platform IME animation a callback can carry the current
        // animated inset while rootWindowInsets already carries the target.
        // Use the larger one on entry; on exit the animated value remains the
        // larger one and lets the surface follow the keyboard down.
        final WindowInsets rootInsets = getRootWindowInsets();
        final int imeInset = Math.max(imeInsetPx(insets),
                rootInsets != null ? imeInsetPx(rootInsets) : 0);
        final Rect padding = chromeInsets(insets, imeInset);
        if (chromeRoot.getPaddingLeft() != padding.left
                || chromeRoot.getPaddingTop() != padding.top
                || chromeRoot.getPaddingRight() != padding.right
                || chromeRoot.getPaddingBottom() != padding.bottom) {
            chromeRoot.setPadding(padding.left, padding.top, padding.right, padding.bottom);
            overlayRoot.setPadding(padding.left, padding.top, padding.right, padding.bottom);
        }
        // The keyboard is root-owned padding. The right host keeps the same
        // vertical grammar and scrolls inside its fixed external geometry when
        // less height is available.
        final boolean keyboard = imeInset > 0;
        if (keyboard != keyboardVisible) {
            keyboardVisible = keyboard;
            renderTrailingHost(isSculpting());
            applyPrimarySurfaceChromePolicy();
        }
    }

    /** How much of the window the soft keyboard is taking, in pixels. */
    private static int imeInsetPx(WindowInsets insets) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            return insets.getInsets(WindowInsets.Type.ime()).bottom;
        }
        // Before R there is no way to tell the keyboard apart from the
        // navigation bar in an inset, so this reports none: the pre-R path keeps
        // the layout it always had rather than guessing.
        return 0;
    }

    private Rect chromeInsets(WindowInsets insets, int imeInsetPx) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            final android.graphics.Insets bars = insets.getInsets(
                    WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout());
            // The keyboard replaces the navigation bar rather than adding to
            // it: they occupy the same edge, and adding both would leave a
            // visible dead band above the keys.
            return new Rect(bars.left, bars.top, bars.right,
                    Math.max(bars.bottom, imeInsetPx));
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
        // A tall window may give the status capsule a second toolbar line. That
        // line belongs to the leading/global region and must not redefine the
        // fixed top of the unrelated right context or direct Sculpt controls.
        // Resolve the margin during measurement, then remeasure once if it
        // changed so this traversal has the final geometry.
        final int controlsHeight = EditorControlStyles.dimen(getContext(),
                R.dimen.toolbar_height);
        final int expansion = Math.max(0, toolbar.getMeasuredHeight() - controlsHeight);
        final boolean hostChanged = trailingHost.setUpstreamToolbarExpansionPx(expansion);
        final LinearLayout.LayoutParams brushParams =
                (LinearLayout.LayoutParams) brushControls.getLayoutParams();
        final int brushTop = EditorControlStyles.dimen(getContext(), R.dimen.row_gap)
                - expansion;
        final boolean brushChanged = brushParams.topMargin != brushTop;
        if (brushChanged) {
            brushParams.topMargin = brushTop;
        }
        if (hostChanged || brushChanged) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }
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
        shortWindow = heightDp < WorkspaceLayoutMode.LOW_HEIGHT_MAX_DP;
        // Roughly half the window's height for the two brush tracks, bounded by
        // the control's own sensible range, so they shrink with the window
        // instead of being clipped by it.
        brushControls.setTrackHeightPx(Math.round(heightPx * 0.45f));

        objectsColumnAffordable = layoutMode.objectsDocked(widthDp);
        applyObjectsPlacement();
        trailingHost.refreshParentPlacement();
        placeInspector(layoutMode.inspectorPlacement(heightDp), widthDp, heightDp);
        renderTrailingHost(isSculpting());
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
            dismissPrimarySurfacesExcept(objectsPopover);
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
            // Always on the shapes: a plane is a question that follows New
            // Sketch, and the palette must not reopen part way through it.
            addPrimitivePalette.showPlanes(false);
            dismissPrimarySurfacesExcept(addPrimitivePalette);
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

        final String body = BodyLabels.of(context, created);
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
     *
     * <p>An open side-placed precision surface belongs to the same trailing
     * region and is measured with the host. It used to sit outboard of the
     * cluster, so clamping to the cluster cleared it for free; now that the
     * host keeps the trailing edge and the panel is seated inboard of it, the
     * limit is the leading edge of whichever of the two comes first — otherwise
     * the crescent this rule exists to prevent simply reappears under an open
     * panel instead of under the rail.
     */
    private int trailingLimitFor(View invoker, int width, int gap) {
        final int windowLimit = getWidth() - gap;
        if (!trailingHost.isShown() || isInTrailingCluster(invoker)) {
            return windowLimit;
        }
        int limit = windowLimit;
        for (View surface : new View[]{trailingHost, sideInspector()}) {
            if (surface == null) {
                continue;
            }
            final Rect bounds = new Rect(0, 0, surface.getWidth(), surface.getHeight());
            offsetDescendantRectToMyCoords(surface, bounds);
            limit = Math.min(limit, bounds.left - gap);
        }
        // Never tighter than the surface's own width: a window too narrow to
        // seat it beside the cluster is still laid out, at the leading edge,
        // rather than at a negative margin.
        return Math.max(limit, width);
    }

    /** The precision surface while it is laid out beside the model, else null. */
    private View sideInspector() {
        return inspectorPlacement != WorkspaceLayoutMode.InspectorPlacement.BOTTOM_SHEET
                && inspector.getParent() == middleRow && inspector.isShown()
                ? inspector : null;
    }

    /** Whether this control is part of the trailing tool cluster. */
    private boolean isInTrailingCluster(View control) {
        for (View view = control; view != null; ) {
            if (view == trailingHost) {
                return true;
            }
            final ViewParent parent = view.getParent();
            view = parent instanceof View ? (View) parent : null;
        }
        return false;
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
            // The gap remains meaningful during the surface's first measured
            // frame and when the lower row returns after dismissal. No control
            // ever sits directly on the sheet boundary.
            sheetParams.topMargin = inset;
            sheetParams.bottomMargin = inset;
            chromeRoot.addView(inspector, sheetParams);
            applyPrimarySurfaceChromePolicy();
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
        // The gap to the right host, not an inset from the trailing window edge.
        //
        // The trailing window edge belongs to the right host and to nothing
        // else. Appending this panel to the row put it AFTER the host, and a
        // laid-out sibling in a horizontal row costs width: the host was pushed
        // inward by the panel's own width plus these margins, so its 8 dp
        // resting inset became about 320 dp in a short landscape window and
        // about 360 dp on a tablet for the whole time Exact or Details was
        // open. The host is the one fixed external surface of the workspace and
        // a panel opening beside it may not translate it.
        //
        // So the panel is seated BEFORE the host, taking width the weighted
        // spacer was holding, and this margin is the standoff between the two
        // surfaces — the same gap every anchored surface keeps off the cluster
        // (see trailingLimitFor). The host's own right margin is then the only
        // thing deciding where the trailing edge is, in every state.
        params.rightMargin = EditorControlStyles.dimen(context, R.dimen.overlay_anchor_gap);
        params.bottomMargin = EditorControlStyles.dimen(context, R.dimen.row_gap);
        middleRow.addView(inspector, middleRow.indexOfChild(trailingHost), params);
        applyPrimarySurfaceChromePolicy();
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
            projectPopover.closeImmediately();
            toolbar.showProjectActionsOpen(false);
            objectsPopover.closeImmediately();
            objectsCapsule.showObjectsOpen(false);
            addPrimitivePalette.closeImmediately();
            objectsCapsule.showAddOpen(false);
            setPrecisionOpen(false);
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
        // Bracketed even though this branch mutates nothing, and deliberately.
        // Both answers to the start question are the same moment — the session
        // being seeded — and the boundary is what states its postcondition: an
        // empty Construction history before the user's first act. Making only
        // the branch that happens to mutate carry it would make the invariant
        // depend on which answer was pressed.
        NativeViewport.beginSessionInitialization();
        NativeViewport.endSessionInitialization();
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

        // Everything this branch does to reach a sculptable mesh is SESSION
        // SEEDING, not a user edit. Shaping the body into a sphere is a real
        // Construction change and would otherwise be recorded — and the user's
        // first Undo would then rewind the answer they gave to the start
        // question rather than anything they did. The boundary is closed on
        // every path out of here, refusal included, so a failed start cannot
        // leave the session inside it.
        NativeViewport.beginSessionInitialization();
        final double[] primitive = new double[NativeViewport.PRIMITIVE_STATE_SIZE];
        NativeViewport.constructionPrimitive(primitive);
        final int applied = NativeViewport.applyConstructionSphere(
                primitive[NativeViewport.PRIMITIVE_SPHERE_DIAMETER]);
        // UNCHANGED is a success: it means the body was already exactly this
        // sphere, which is a perfectly good thing to sculpt.
        final boolean shaped = applied == NativeViewport.APPLY_APPLIED
                || applied == NativeViewport.APPLY_UNCHANGED;
        if (!shaped || NativeViewport.freezeToSculpt() != NativeViewport.SCULPT_OK) {
            NativeViewport.endSessionInitialization();
            syncFromNative();
            showStatus(getContext().getString(R.string.status_sculpt_start_failed),
                    R.attr.fsTextError);
            return;
        }
        NativeViewport.endSessionInitialization();
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
        // An Imported Mesh is not derived from parameters and nothing may
        // invent a primitive for it, so Shape has no answer for one: that entry
        // is ABSENT for one rather than drawn and then refused, and the guard
        // stays in the domain because withdrawing a control is not removing it.
        //
        // Start Sculpting is a different matter since `IMPORT-01B`. An imported
        // body can be sculpted -- the seed is its own geometry -- so the
        // transition is OFFERED for one, and what the representation still
        // decides is only the WORDING: which context this is, and where the way
        // back out of Sculpt says it goes.
        final boolean imported = NativeViewport.sceneActiveBodyIsImported();
        if (imported && uiState.constructionTool() == EditorUiState.CONSTRUCTION_TOOL_SHAPE) {
            // The rail entry the user was holding is about to be withdrawn.
            // Moving them to Transform — which an imported body fully supports
            // — rather than leaving a held tool with no entry, so the precision
            // surface and the gizmo agree with the rail.
            uiState.setConstructionTool(EditorUiState.CONSTRUCTION_TOOL_TRANSFORM);
        }
        // The sketch session (CAD-R0-A1A2) and the active body's third
        // representation, both native truth read here and nowhere remembered.
        NativeViewport.sketchState(nativeSketch);
        final int sketchState = (int) nativeSketch[NativeViewport.SKETCH_STATE];
        final boolean sketching = sketchState != NativeViewport.SKETCH_INACTIVE;
        final boolean cad = NativeViewport.sceneActiveBodyIsCad();

        toolbar.showContext(sculpting, hasFrozenMesh, imported, cad, sketchState,
                (int) nativeSketch[NativeViewport.SKETCH_PLANE]);
        toolbar.showEditingTransitions(true);
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
        // And not while sketching either: the scene holds still until the
        // sketch commits or is cancelled, and both acts are refused below JNI
        // on those terms.
        objectsCapsule.showCreationAvailable(!sculpting && !sketching);
        objectsSection.showCreationAvailable(!sculpting && !sketching);
        // And deletion on the same terms, for the same reason: `sceneDeleteBody`
        // refuses while sculpting, because the Sculpt target is fixed for the
        // duration of the mode and Undo is refused there too -- a delete made
        // there could not be taken back until the user left. The other half of
        // the answer, that the last body cannot go, is the list's own and comes
        // from the scene's size.
        objectsSection.showDeletionAvailable(!sculpting && !sketching);
        if (sculpting || sketching) {
            // The palette is anchored to a control that has just gone. Left open
            // it would stand on the model attached to nothing, and choosing a
            // tile from it would reach exactly the refusal this removes.
            setAddPrimitiveOpen(false, null);
        }

        if (sculpting) {
            brushControls.setVisibility(VISIBLE);
            brushControls.refreshFromNative();
            sculptContext.refreshFromNative();
        } else {
            // In Construction the brush controls are not merely disabled but
            // absent: there is no brush to set, and an inert slider standing on
            // the model would be pure occlusion.
            brushControls.setVisibility(GONE);
            shapeEditor.refreshFromNative();
            placementEditor.refreshFromNative();
            sketchEditor.refreshFromNative();
            cadEditor.refreshFromNative();
        }
        objectsCapsule.refreshFromNative();
        refreshTransformGizmo(sculpting);
        refreshHistoryControls();
        showActiveInspectorBody(sculpting);
        showPrecisionToggle(sculpting);
        showDefaultStatus(sculpting);
        applyImportedPreviewChrome();
    }

    /**
     * Withdraws every editing control while an imported file is on the screen.
     *
     * <p>GLB-IMPORT-R0, and it is about the DIAGNOSTIC preview — not an
     * imported body, which is an ordinary object with ordinary chrome. The
     * preview is not editable — it has no body, no primitive, no sculpt mesh
     * and no history — so Shape, Transform, Start
     * Sculpting, the exact values, creation, the handles and Undo/Redo would
     * every one of them be a control that cannot succeed, pointed at a model
     * the user cannot currently see. They are ABSENT rather than disabled, and
     * they come back untouched when the model does: nothing about the workspace
     * state is recomputed here, only its visibility.
     *
     * <p>The viewport, the camera and Display stay live, because looking at the
     * imported mesh from another angle is the entire point of showing it.
     */
    private void applyImportedPreviewChrome() {
        if (!NativeViewport.glbPreviewVisible()) {
            // Nothing to do. syncFromNative has already put every control back
            // exactly where the mode, the window and the scene say it belongs,
            // so leaving the preview restores the accepted workspace rather
            // than a remembered copy of it.
            return;
        }
        // Closing comes FIRST. Each of these re-renders the trailing host as a
        // side effect of putting a surface away, so withdrawing the host before
        // them would simply be undone — which is exactly what the first run of
        // `GLBIR0-19` caught.
        setPrecisionOpen(false);
        setAddPrimitiveOpen(false, null);
        setObjectsPanelOpen(false);

        // The Tool Rail is a member of the trailing host, so withdrawing the
        // host withdraws the rail, the transform selectors and the precision
        // trigger together — one surface, one decision.
        trailingHost.setVisibility(GONE);
        objectsCapsule.setVisibility(GONE);
        objectsSection.setVisibility(GONE);
        historyGroup.setVisibility(GONE);
        // The Property Inspector is CLOSED above rather than hidden here, and
        // the difference matters: it is an AnchoredSurfaceView that owns its
        // own visibility as its open/closed state, and nothing else in the
        // workspace ever writes that field. Setting it GONE from here is an
        // override with no owner to undo it — which is exactly what the first
        // full-suite run caught, as an inspector that stayed gone for the rest
        // of the process and a later IME case finding it in the wrong place.
        // Every other view above has its normal value rewritten by
        // syncFromNative on the way past, so hiding those is safe.
        // Start Sculpting, Resume Sculpt and Back to Construction all act on
        // the active body, which is not the thing on the screen. The utility
        // group stays — Display still applies to the viewport, and the project
        // control is how the user gets back.
        toolbar.showEditingTransitions(false);
    }

    /**
     * Tells native code whether there is a gizmo, and draws the selector to
     * match what it reports back.
     *
     * <p><b>This layer owns WHEN, and nothing else.</b> There is a gizmo exactly
     * when the product is in Construction, the held rail entry is Transform, and
     * a body exists to act on — three workspace facts, which is why they are
     * decided here. Where the handles are, how large they are on screen, which
     * one a touch lands on and what a drag means are all camera and placement
     * questions with one owner below JNI, and none of them is answered, cached
     * or second-guessed on this side.
     *
     * <p>The selector is <b>absent</b> rather than disabled everywhere there is
     * no gizmo, for the same reason creation is absent while sculpting: a
     * control that cannot succeed is not drawn. And which of Move or Rotate is
     * drawn active is read back from the session after the request, exactly as
     * the Tool Rail reads back the held brush, so the pair can never claim a
     * mode the session is not in.
     */
    private void refreshTransformGizmo(boolean sculpting) {
        // The display's own scale, pushed from the one place that knows it.
        // Native code sizes the handles and their hit corridors in reference
        // units — what Android calls dp — and this is the only number this side
        // contributes to that. Pushed on every refresh rather than once, because
        // a window that moves to another display changes it and an atomic float
        // store is not worth a lifecycle hook to avoid.
        NativeViewport.setGizmoPixelScale(getResources().getDisplayMetrics().density);
        // Not while sketching: the viewport is the sketch plane, and a handle
        // over it would be pointing at a body the sketch is not about.
        final boolean offered = !sculpting && !isSketching()
                && uiState.constructionTool() == EditorUiState.CONSTRUCTION_TOOL_TRANSFORM
                && NativeViewport.sceneActiveBodyId() != NativeViewport.NO_OBJECT;
        NativeViewport.setGizmoActive(offered);
        if (offered) {
            NativeViewport.gizmoState(nativeGizmo);
        }
        renderTrailingHost(sculpting);
    }

    /** Supplies the host one derived snapshot; native reads and commands stay here. */
    private void renderTrailingHost(boolean sculpting) {
        NativeViewport.sketchState(nativeSketch);
        final int sketchState = (int) nativeSketch[NativeViewport.SKETCH_STATE];
        final boolean sketching = sketchState != NativeViewport.SKETCH_INACTIVE;
        final boolean transformOffered = !sculpting && !sketching
                && uiState.constructionTool() == EditorUiState.CONSTRUCTION_TOOL_TRANSFORM
                && NativeViewport.sceneActiveBodyId() != NativeViewport.NO_OBJECT;
        if (transformOffered) {
            NativeViewport.gizmoState(nativeGizmo);
        }
        final int activeTool = sketching
                ? (int) nativeSketch[NativeViewport.SKETCH_TOOL]
                : sculpting
                        ? (int) nativeSculpt[NativeViewport.SCULPT_TOOL]
                        : uiState.constructionTool();
        final int transformMode = transformOffered
                ? (int) nativeGizmo[NativeViewport.GIZMO_MODE]
                : NativeViewport.GIZMO_MODE_MOVE;
        final boolean transformSpaceOffered = transformOffered
                && nativeGizmo[NativeViewport.GIZMO_SPACE_SELECTABLE] != 0.0;
        final int transformSpace = transformSpaceOffered
                ? (int) nativeGizmo[NativeViewport.GIZMO_SPACE]
                : NativeViewport.GIZMO_SPACE_LOCAL;
        trailingHost.render(new WorkspaceTrailingHostView.PresentationState(
                sculpting,
                activeTool,
                transformOffered,
                transformMode,
                transformSpaceOffered,
                transformSpace,
                uiState.precisionOpen(sculpting),
                getContext().getString(precisionSurfaceName(sculpting)),
                shortWindow,
                displayPopover != null && displayPopover.isOpen(),
                // Shape has no answer for an Imported Mesh, so the rail does
                // not offer it for one. Asked from native truth rather than
                // remembered, exactly like every other fact this snapshot
                // carries. A CAD Body keeps it: Shape is what the body IS, and
                // for one that is its sketch and its extrusion.
                !NativeViewport.sceneActiveBodyIsImported(),
                sketchState));
    }

    /**
     * Asks for Move, Rotate or Scale, then redraws from what the session
     * reports.
     *
     * <p>Costs the model nothing: no mesh revision, no geometry publication and
     * no history step. It deliberately does not re-read the exact-value editors
     * either — nothing about the object changed, and a refresh would discard a
     * half-typed draft for a presentation-only act.
     *
     * <p>Entering and leaving Scale moves the SPACE as well, which is why the
     * redraw reads both back rather than only the mode: the session owns that
     * coupling and this layer only shows what it decided.
     */
    @Override
    public void onTransformModeRequested(int mode) {
        NativeViewport.setGizmoMode(mode);
        refreshTransformGizmo(false);
    }

    /**
     * Asks for World or Local, then redraws from what the session reports.
     *
     * <p>Exactly as cheap as the mode: presentation state, no revision, no
     * publication, no history step, and no re-read of the exact-value editors.
     */
    @Override
    public void onTransformSpaceRequested(int space) {
        NativeViewport.setGizmoSpace(space);
        refreshTransformGizmo(false);
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
     * <p>The pair is now drawn in <b>both</b> modes. It was withdrawn in Sculpt
     * for as long as sculpting had no undo, because a greyed Undo beside a
     * stroke would have read as "your stroke can be taken back, just not yet".
     * Since {@code ARCH-OWNER-12} it can be, so the controls mean exactly what
     * they look like in either mode: in Construction they step the project
     * history, and in Sculpt they step the active body's stroke history.
     *
     * <p>Which of the two a tap means is decided in native code and never
     * here — see {@link NativeViewport#historyUndo}. This method deliberately
     * takes no mode argument and reads none: the enabled state comes from the
     * SAME dispatching query the tap will, so there is no branch here that
     * could route a tap one way and light the control the other. It used to
     * take a {@code sculpting} flag, and that flag existed only to withdraw
     * the pair.
     */
    private void refreshHistoryControls() {
        // Withdrawn while sketching: a sketch in progress is not in the
        // history yet, and a Construction step under it is refused below JNI.
        historyGroup.setVisibility(isSketching() ? GONE : VISIBLE);
        undoAction.setEnabled(NativeViewport.historyUndoAvailable());
        redoAction.setEnabled(NativeViewport.historyRedoAvailable());
    }

    /**
     * Steps whichever history the product mode says these controls mean.
     *
     * <p>The choice is native — in Sculpt this is the active body's stroke
     * history, and everywhere else the Construction history — so this method
     * asks for a step and reports what came back rather than deciding anything.
     *
     * <p>The control's enabled state already answers whether there is a step, so
     * the ordinary outcome is silent: the model changes, the exact-value editors
     * re-read, and nothing is written to the status line. Only a refusal says
     * anything, because a control that did nothing and said nothing would be
     * the defect this reports.
     */
    private void onHistoryStepRequested(boolean forward) {
        final int status = forward ? NativeViewport.historyRedo() : NativeViewport.historyUndo();
        finishEditing();
        onNativeStateChanged();
        if (status == NativeViewport.HISTORY_REFUSED_IN_SCULPT) {
            showStatus(getContext().getString(R.string.status_history_refused_in_sculpt),
                    R.attr.fsTextError);
        } else if (status == NativeViewport.HISTORY_STROKE_ACTIVE) {
            showStatus(getContext().getString(R.string.status_history_stroke_active),
                    R.attr.fsTextError);
        }
        reportUnretainedStrokes();
    }

    /**
     * Says so, once, when a stroke was too large for the sculpt history to hold.
     *
     * <p>A standing fault would be the wrong shape for this: it is news at the
     * moment it becomes true and then it is history. So the count is compared
     * against the last one seen and reported only when it moves — which is also
     * why the check is cheap enough to sit on the ordinary refresh path.
     *
     * <p>This is the one user-visible consequence of the history's byte budget.
     * The stroke itself applied normally; what it cannot do is be taken back,
     * and a user tapping Undo at geometry that will not move deserves to be
     * told why rather than left to conclude the control is broken.
     */
    private void reportUnretainedStrokes() {
        final long count = NativeViewport.sculptHistoryNotRetainedCount();
        if (count > unretainedStrokesReported) {
            unretainedStrokesReported = count;
            showStatus(getContext().getString(R.string.status_sculpt_stroke_not_retained),
                    R.attr.fsTextError);
        } else {
            // Falls as well as rises: the counter is per body, so switching to
            // a body that has never overflowed must not leave this armed to
            // report the next stroke there.
            unretainedStrokesReported = count;
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
        if (isSketching()) {
            // The sketch's own surface, titled by its plane: it edits the one
            // sketch in progress, which belongs to no body yet.
            inspector.setBody(sketchEditor, context.getString(R.string.inspector_sketch_title,
                    context.getString(CadFeatureEditorView.planeName(
                            (int) nativeSketch[NativeViewport.SKETCH_PLANE]))));
            return;
        }
        final String body = BodyLabels.ofActive(context);
        if (uiState.constructionTool() == EditorUiState.CONSTRUCTION_TOOL_TRANSFORM) {
            inspector.setBody(placementEditor,
                    context.getString(R.string.inspector_place_title_for_body, body));
        } else if (NativeViewport.sceneActiveBodyIsCad()) {
            // Shape, for a CAD Body, is its sketch and its extrusion.
            inspector.setBody(cadEditor,
                    context.getString(R.string.inspector_shape_title_for_body, body));
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
        final boolean open = uiState.precisionOpen(sculpting);
        renderTrailingHost(sculpting);
        applyPrecisionOpen(open);
    }

    /** What the precision toggle opens, given the mode and the held entry. */
    private int precisionSurfaceName(boolean sculpting) {
        if (sculpting) {
            return R.string.precision_sculpt;
        }
        if (isSketching()) {
            return R.string.precision_sketch;
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
    @Override
    public void onPrecisionRequested() {
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
            dismissPrimarySurfacesExcept(inspector);
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

    /** Whether the one native sketch session is open, read fresh every time. */
    private boolean isSketching() {
        NativeViewport.sketchState(nativeSketch);
        return nativeSketch[NativeViewport.SKETCH_STATE] != NativeViewport.SKETCH_INACTIVE;
    }

    // -----------------------------------------------------------------------
    // The sketch (CAD-R0-A1A2)
    // -----------------------------------------------------------------------
    //
    // Every act here asks the one native session and re-reads. The workspace
    // holds no sketch: not an entity, not a profile, not a depth. What it owns
    // is which chrome is drawn for the session's state, and what the status
    // line says about a refusal.

    /** The palette's New Sketch, with a plane chosen. */
    @Override
    public void onNewSketchChosen(int workplane) {
        final Context context = getContext();
        final int status = NativeViewport.sketchBegin(workplane);
        setAddPrimitiveOpen(false, null);
        if (status != NativeViewport.CAD_OK) {
            showStatus(CadStatusMessages.describe(context, status), R.attr.fsTextError);
            return;
        }
        // A sketch is its own context: the surfaces of the one it replaces are
        // put away, and the view the session framed is what the user sees.
        dismissPrimarySurfacesExcept(null);
        finishEditing();
        lastReportedSketchStatus = NativeViewport.CAD_OK;
        onNativeStateChanged();
        NativeViewport.sketchState(nativeSketch);
        final int tool = (int) nativeSketch[NativeViewport.SKETCH_TOOL];
        showStatus(context.getString(R.string.status_sketch_started,
                context.getString(CadFeatureEditorView.planeName(workplane)),
                context.getString(SKETCH_TOOL_HINTS[tool])), R.attr.fsTextSecondary);
    }

    /**
     * Enters spatial "Choose Sketch Support" (`CAD-A3`): the three world planes
     * become touchable targets and, in a CAD project, the planar faces of CAD
     * bodies too. The user taps a target to highlight it and taps it again to
     * begin the sketch there; System Back cancels. The primary, viewport-first
     * path; the by-name plane list stays as the fallback.
     */
    @Override
    public void onNewSketchSpatial() {
        final Context context = getContext();
        // Faces are eligible whenever there is a CAD body to sketch on; world
        // planes are always available. Passing true is safe -- a non-CAD body
        // simply resolves to no eligible face.
        final boolean started = NativeViewport.supportChooserBegin(true);
        setAddPrimitiveOpen(false, null);
        if (!started) {
            showStatus(context.getString(R.string.status_sketch_unavailable),
                    R.attr.fsTextError);
            return;
        }
        dismissPrimarySurfacesExcept(null);
        finishEditing();
        onNativeStateChanged();
        showStatus(context.getString(R.string.status_support_chooser), R.attr.fsTextSecondary);
    }

    /** A sketch gesture ended: the entity list, the selection or a refusal. */
    private void onSketchGestureSettled() {
        sketchEditor.refreshFromNative();
        NativeViewport.sketchState(nativeSketch);
        final int last = (int) nativeSketch[NativeViewport.SKETCH_LAST_STATUS];
        if (last != NativeViewport.CAD_OK && last != lastReportedSketchStatus) {
            showStatus(CadStatusMessages.describe(getContext(), last), R.attr.fsTextError);
        }
        lastReportedSketchStatus = last;
    }

    @Override
    public void onFinishSketchRequested() {
        final Context context = getContext();
        final int status = NativeViewport.sketchFinish();
        if (status != NativeViewport.CAD_OK) {
            showStatus(CadStatusMessages.describe(context, status), R.attr.fsTextError);
            return;
        }
        finishEditing();
        syncFromNative();
        // The depth and the profile choice are the next act, so the surface
        // that holds them opens without being asked for.
        setPrecisionOpen(true);
        NativeViewport.sketchState(nativeSketch);
        final int profiles = (int) nativeSketch[NativeViewport.SKETCH_PROFILE_COUNT];
        showStatus(profiles > 1
                        ? context.getString(R.string.status_sketch_finished_choose, profiles)
                        : context.getString(R.string.status_sketch_finished),
                R.attr.fsTextSuccess);
    }

    /** The toolbar's Extrude and the precision surface's Extrude: one act. */
    @Override
    public void onExtrudeRequested() {
        final Context context = getContext();
        final long created = NativeViewport.sketchCommit();
        if (created == NativeViewport.NO_OBJECT) {
            showStatus(CadStatusMessages.describe(context, NativeViewport.sketchLastStatus()),
                    R.attr.fsTextError);
            return;
        }
        dismissPrimarySurfacesExcept(null);
        finishEditing();
        // The new body is active and its Shape is what the user just made; the
        // rail lands there rather than wherever it was before the sketch.
        uiState.setConstructionTool(EditorUiState.CONSTRUCTION_TOOL_SHAPE);
        onNativeStateChanged();
        showStatus(context.getString(R.string.status_sketch_extruded,
                BodyLabels.of(context, created)), R.attr.fsTextSuccess);
    }

    @Override
    public void onCancelSketchRequested() {
        NativeViewport.sketchCancel();
        dismissPrimarySurfacesExcept(null);
        finishEditing();
        onNativeStateChanged();
        showStatus(getContext().getString(R.string.status_sketch_cancelled),
                R.attr.fsTextSecondary);
    }

    @Override
    public void onBackToSketchRequested() {
        NativeViewport.sketchBackToEditing();
        finishEditing();
        syncFromNative();
    }

    SketchEditorView sketchEditor() {
        return sketchEditor;
    }

    CadFeatureEditorView cadEditor() {
        return cadEditor;
    }

    // -----------------------------------------------------------------------
    // Tool Rail
    // -----------------------------------------------------------------------

    @Override
    public void onToolSelected(int key) {
        if (isSketching()) {
            // Ask, then read back, exactly as the sculpt brushes do: the rail
            // draws the tool the session reports. Changing tool ends a polyline
            // being placed and drops a drag; it touches no project state.
            NativeViewport.sketchSetTool(key);
            final int active = NativeViewport.sketchTool();
            renderTrailingHost(false);
            sketchEditor.refreshFromNative();
            final int index = (active >= 0 && active < SKETCH_TOOL_NAMES.length)
                    ? active : NativeViewport.SKETCH_TOOL_RECTANGLE;
            showStatus(getContext().getString(R.string.status_sketch_tool,
                    getContext().getString(SKETCH_TOOL_NAMES[index]),
                    getContext().getString(SKETCH_TOOL_HINTS[index])), R.attr.fsTextSecondary);
            finishEditing();
            return;
        }
        if (isSculpting()) {
            // Ask, then read back: the rail draws the tool native code reports,
            // not the one that was tapped. Changing tool touches no geometry --
            // it publishes no revision, uploads nothing, does not re-freeze and
            // leaves Radius and Strength exactly as they were, because both are
            // shared by every tool.
            NativeViewport.setSculptTool(key);
            final int active = NativeViewport.sculptTool();
            nativeSculpt[NativeViewport.SCULPT_TOOL] = active;
            renderTrailingHost(true);
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
        showActiveInspectorBody(false);
        // Transform is the entry that owns direct manipulation, so the handles
        // and their Move/Rotate selector arrive with it and leave with it. This
        // publishes no geometry and records no history: it only tells the one
        // native session whether there is a gizmo at all.
        refreshTransformGizmo(false);
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
        projectPopover.setMotionAllowed(allowed);
    }

    // -----------------------------------------------------------------------
    // Global actions
    // -----------------------------------------------------------------------

    /**
     * <b>Start Sculpting.</b> Copies the active body's current SOURCE mesh into
     * a sculpt mesh and enters Sculpt Mode.
     *
     * <p>Which source that is, is the body's representation and is decided below
     * JNI: a Construction Body regenerates its primitive, an Imported Mesh hands
     * over the geometry it owns (`IMPORT-01B`). Neither is written by the copy,
     * and neither is written by any stroke afterwards.
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
        final int status = NativeViewport.freezeToSculpt();
        if (status != NativeViewport.SCULPT_OK) {
            // The CAD refusal has its own sentence: the control is absent for
            // a CAD Body, so reaching it means a stale surface, and the user
            // deserves the real reason rather than a generic failure.
            showStatus(getContext().getString(status == NativeViewport.SCULPT_REFUSED_CAD_BODY
                            ? R.string.status_cad_no_sculpt
                            : R.string.status_sculpt_prepare_failed),
                    R.attr.fsTextError);
            return;
        }
        // A primary surface belongs to the mode that opened it. In particular,
        // Display temporarily suspends the rail, so carrying it into Sculpt
        // would hide the very controls needed to continue the workflow.
        dismissPrimarySurfacesExcept(null);
        finishEditing();
        syncFromNative();
        // The verdict names what was KEPT. A Construction Body's primitive is
        // still there to name; an imported body has none, and saying so in
        // Construction vocabulary would claim a shape the user never made.
        showStatus(NativeViewport.sceneActiveBodyIsImported()
                        ? getContext().getString(R.string.status_now_sculpting_imported)
                        : getContext().getString(R.string.status_now_sculpting,
                                shapeEditor.describeNativeKind()),
                R.attr.fsTextSuccess);
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
        dismissPrimarySurfacesExcept(null);
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
     * Back to the body's SOURCE representation, which sculpting never wrote.
     *
     * <p>The wording elsewhere changed at UI-R4B and again at `IMPORT-01B`, and
     * what this does did not. A Construction Body's exact primitive, its
     * parameters and its placement, or an Imported Mesh's positions, normals,
     * topology and submesh batches, are exactly what they were before Start
     * Sculpting — no sculpt edit has ever been allowed to reach either — and
     * Resume Sculpt returns to the same mesh with the same revision, the same
     * counts and the same stroke history. The user reads the control as Back to
     * Construction or Back to Imported Mesh; the one native act is the same.
     */
    @Override
    public void onBackToConstruction() {
        NativeViewport.enterConstructionMode();
        dismissPrimarySurfacesExcept(null);
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
            dismissPrimarySurfacesExcept(displayPopover);
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

    // -----------------------------------------------------------------------
    // The project
    // -----------------------------------------------------------------------
    //
    // Three methods, and between them the whole of the product's persistence
    // UI. The workspace owns none of the format and none of the storage: it
    // asks native code for bytes, hands them to {@link ProjectSlot}, and writes
    // one verdict to the one status line. Every branch below says what happened
    // to the USER'S WORK, because that is the only thing at stake in a save or
    // an open — which is why every failure message ends by saying the work is
    // unchanged, and why it can say so truthfully: the native load is
    // fail-closed.

    @Override
    public void onProjectActionsRequested() {
        final boolean opening = !projectPopover.isOpen();
        if (opening) {
            dismissPrimarySurfacesExcept(projectPopover);
            // Read from disk every time it opens rather than from a cached
            // flag: the slot could have been written a moment ago by this
            // session, or could have been there since before the process
            // started, and Open must never offer to do something it cannot.
            projectPopover.showSlotState(ProjectSlot.exists(getContext()));
            // Hang the surface below the toolbar's ACTUAL height, for the same
            // reason the display popover does: the toolbar grows a second line
            // when the status message cannot share the control row, and a fixed
            // offset would put the panel on top of the very message a failed
            // save writes.
            final ViewGroup.MarginLayoutParams params =
                    (ViewGroup.MarginLayoutParams) projectPopover.getLayoutParams();
            final int toolbarHeight = toolbar.getHeight();
            if (toolbarHeight > 0 && params.topMargin != toolbarHeight) {
                params.topMargin = toolbarHeight;
                projectPopover.setLayoutParams(params);
            }
        }
        projectPopover.setOpen(opening);
        toolbar.showProjectActionsOpen(opening);
    }

    @Override
    public void onSaveProjectRequested() {
        setProjectPanelOpen(false);
        final byte[] bytes = NativeViewport.encodeProject();
        if (bytes == null || bytes.length == 0
                || !ProjectSlot.write(getContext(), bytes)) {
            showStatus(getContext().getString(R.string.status_project_save_failed),
                    R.attr.fsTextError);
            return;
        }
        // The work the checkpoint was protecting is now in the slot the user
        // named, so the checkpoint has nothing left to protect and is retired.
        // Autosave writes a fresh one the moment the project changes again.
        ProjectCheckpoint.clear(getContext());
        autosave.noteProjectPersisted();
        Diagnostics.info(DiagnosticLog.CAT_PERSISTENCE, "MANUAL_SAVE",
                "bytes=" + bytes.length);
        // Saving reads the model and writes a file. It publishes no mesh, mints
        // no revision, changes no mode and records no history step, so nothing
        // on screen has to be re-read afterwards.
        showStatus(getContext().getString(R.string.status_project_saved, projectSummary()),
                R.attr.fsTextSuccess);
    }

    @Override
    public void onOpenProjectRequested() {
        setProjectPanelOpen(false);
        final byte[] bytes = ProjectSlot.read(getContext());
        if (bytes == null) {
            showStatus(getContext().getString(R.string.status_project_none), R.attr.fsTextError);
            return;
        }
        final int status = NativeViewport.loadProject(bytes);
        if (status != NativeViewport.PROJECT_OK) {
            // Nothing to refresh: a refused load changed nothing below JNI, so
            // re-reading would repaint the same values it already shows.
            showStatus(getContext().getString(projectFailureMessage(status)),
                    R.attr.fsTextError);
            return;
        }
        // Everything on screen is now describing a scene that no longer exists:
        // the active body, its dimensions, its placement, the mode, the scene
        // list and whether Undo is available have all been replaced at once.
        // One re-read from native truth answers all of it, which is exactly
        // what syncFromNative is for.
        // The live project and the internal slot now agree, so the next
        // checkpoint of unchanged work is free rather than merely fast. The
        // checkpoint is retired for the same reason a Save retires it.
        ProjectCheckpoint.clear(getContext());
        autosave.noteProjectPersisted();
        Diagnostics.info(DiagnosticLog.CAT_PERSISTENCE, "MANUAL_OPEN",
                "bytes=" + bytes.length);
        dismissPrimarySurfacesExcept(null);
        onNativeStateChanged();
        showStatus(getContext().getString(R.string.status_project_opened, projectSummary()),
                R.attr.fsTextSuccess);
    }

    // -----------------------------------------------------------------------
    // Autosave
    // -----------------------------------------------------------------------
    //
    // The controller owns WHEN; this owns WHERE FROM. Every place the workspace
    // asks native code to change the project tells it the project may have
    // changed, and so does the end of every viewport gesture — a sculpt stroke
    // never passes through Java at all, so without that edge a whole stroke
    // could go unnoticed until the next unrelated edit.
    //
    // Noting is cheap and idempotent by design: it schedules, and the schedule
    // coalesces. Over-noting costs one removeCallbacks; under-noting costs the
    // user their work, so every doubtful case notes.

    /** The autosave controller, so lifecycle and verification can reach it. */
    AutosaveController autosaveController() {
        return autosave;
    }

    /**
     * Tells autosave the project may have changed.
     *
     * <p>Safe to call from anywhere on the UI thread and safe to call too often.
     */
    void noteProjectMaybeDirty() {
        autosave.noteMaybeDirty();
    }

    /**
     * Asks for a checkpoint of the latest state now, without waiting.
     *
     * <p>For the moments where there may be no later: the Activity is stopping,
     * or the renderer has just died.
     */
    void requestImmediateCheckpoint() {
        autosave.requestImmediateCheckpoint();
    }

    /** Lets the worker thread go. Called when the Activity is really finishing. */
    void releaseAutosave() {
        autosave.release();
    }

    // -----------------------------------------------------------------------
    // Recovery
    // -----------------------------------------------------------------------

    /**
     * Offers the recovery question if — and only if — there is something real to
     * offer.
     *
     * <p>A candidate has to survive three tests before the user is troubled with
     * it. There must be a checkpoint file; it must <b>decode through the real
     * decoder</b>, which is what {@code validateProject} runs without applying
     * anything; and the process must not already have settled the question.
     *
     * <p>A checkpoint that fails to decode is <b>quarantined, not retried</b>.
     * That is the whole of the do-not-loop rule: a corrupt candidate left in
     * place would ask the same broken question on every launch forever, so it is
     * moved aside once, reported once, and never offered again.
     *
     * @return whether the question is now on screen
     */
    private boolean offerRecoveryIfPresent() {
        if (!ProjectCheckpoint.exists(getContext())) {
            return false;
        }
        final byte[] bytes = ProjectCheckpoint.read(getContext());
        final int status = bytes == null
                ? NativeViewport.PROJECT_NO_DATA
                : NativeViewport.validateProject(bytes);
        if (status != NativeViewport.PROJECT_OK) {
            // Nothing has been applied and nothing can have been: validate is a
            // decode into temporary state. The live project is whatever a fresh
            // launch built, untouched.
            ProjectCheckpoint.quarantine(getContext());
            uiState.recordRecoveryResolved();
            Diagnostics.warn(DiagnosticLog.CAT_RECOVERY, "CANDIDATE_QUARANTINED",
                    "status=" + status);
            showStatus(getContext().getString(R.string.status_recovery_failed),
                    R.attr.fsTextError);
            return false;
        }
        Diagnostics.info(DiagnosticLog.CAT_RECOVERY, "CANDIDATE_OFFERED",
                "bytes=" + bytes.length);
        recoveryPrompt.setVisibility(VISIBLE);
        return true;
    }

    /** Whether the recovery question is currently on screen. */
    boolean recoveryPromptVisible() {
        return recoveryPrompt.getVisibility() == VISIBLE;
    }

    /**
     * Re-asks the cold-launch recovery question on THIS workspace.
     *
     * <p>Verification only, and it exists because of a real property of the
     * product rather than to work around one. Leaving the foreground
     * checkpoints the live project — that is the point of
     * {@code onStop} — so an Activity recreation writes a checkpoint of its
     * own. A test that planted a specific candidate and then recreated the
     * Activity to see it offered would be racing its own fixture against that
     * write, and would sometimes be asserting about the wrong file.
     *
     * <p>This runs the same {@link #offerRecoveryIfPresent} the constructor runs
     * — the decision under test is identical — without the lifecycle churn
     * around it. The constructor path itself is still covered separately, by the
     * case that recreates the Activity for real.
     *
     * @return whether the question is now on screen
     */
    boolean offerRecoveryForTest() {
        uiState.clearRecoveryResolved();
        return offerRecoveryIfPresent();
    }

    /**
     * Settles the recovery question the way a case that is not about it needs.
     *
     * <p>The same role {@code dismissStartChooserForConstruction} plays, and for
     * the same reason. Leaving the foreground checkpoints the project, so almost
     * every instrumented case leaves a candidate behind — and the first Activity
     * of the next process would then put the recovery question over the chrome
     * that case is trying to measure.
     *
     * <p>The candidate FILE is deliberately left alone: a case that planted one
     * on purpose still has it, and this only records that the question has been
     * answered for this process.
     */
    void dismissRecoveryPromptForTest() {
        uiState.recordRecoveryResolved();
        recoveryPrompt.setVisibility(GONE);
    }

    @Override
    public void onRecoverRequested() {
        uiState.recordRecoveryResolved();
        recoveryPrompt.setVisibility(GONE);

        final byte[] bytes = ProjectCheckpoint.read(getContext());
        final int status = bytes == null
                ? NativeViewport.PROJECT_NO_DATA
                : NativeViewport.loadProject(bytes);
        if (status != NativeViewport.PROJECT_OK) {
            // Between the offer and the press the file became unreadable. The
            // load is fail-closed, so the live project is exactly what it was;
            // the candidate is retired so the next launch does not re-offer it.
            ProjectCheckpoint.quarantine(getContext());
            Diagnostics.error(DiagnosticLog.CAT_RECOVERY, "RECOVER_FAILED",
                    "status=" + status);
            showStatus(getContext().getString(R.string.status_recovery_failed),
                    R.attr.fsTextError);
            showStartChooser(!uiState.startChoiceMade());
            return;
        }
        // Recovered work is the live project now, and the checkpoint has done
        // its job. It is retired rather than kept: leaving it would offer the
        // same work again on the next launch, as though it had been lost twice.
        ProjectCheckpoint.clear(getContext());
        autosave.noteProjectPersisted();
        // The start question is moot — this project already decided which
        // representation it is in, and asking would offer to change it.
        uiState.recordStartChoice();
        dismissPrimarySurfacesExcept(null);
        onNativeStateChanged();
        Diagnostics.info(DiagnosticLog.CAT_RECOVERY, "RECOVERED",
                "bodies=" + NativeViewport.sceneBodyCount());
        showStatus(getContext().getString(R.string.status_recovery_recovered, projectSummary()),
                R.attr.fsTextSuccess);
    }

    @Override
    public void onDiscardRecoveryRequested() {
        uiState.recordRecoveryResolved();
        recoveryPrompt.setVisibility(GONE);
        // The only thing discarded is the checkpoint. The manual slot is not
        // touched, not read and not written by this — which is exactly what the
        // option's description promises the user.
        ProjectCheckpoint.clear(getContext());
        Diagnostics.info(DiagnosticLog.CAT_RECOVERY, "DISCARDED", null);
        showStatus(getContext().getString(R.string.status_recovery_discarded),
                R.attr.fsTextPrimary);
        // Discarding means starting normally, so the question that was deferred
        // to make room for this one gets asked now.
        showStartChooser(!uiState.startChoiceMade());
    }

    // -----------------------------------------------------------------------
    // Project transfer, through the system's own document UI
    // -----------------------------------------------------------------------

    @Override
    public void onSaveCopyRequested() {
        setProjectPanelOpen(false);
        // Encoded before the picker is shown, so a project that cannot be
        // encoded says so immediately instead of after the user has chosen a
        // destination and watched an empty file appear there.
        final byte[] bytes = NativeViewport.encodeProject();
        if (bytes == null || bytes.length == 0) {
            showStatus(getContext().getString(R.string.status_project_no_encode),
                    R.attr.fsTextError);
            return;
        }
        pendingCopyBytes = bytes;
        if (transferHost == null || !transferHost.requestCreateProjectDocument()) {
            pendingCopyBytes = null;
            showStatus(getContext().getString(R.string.status_project_copy_failed),
                    R.attr.fsTextError);
        }
    }

    @Override
    public void onOpenFileRequested() {
        setProjectPanelOpen(false);
        if (transferHost == null || !transferHost.requestOpenProjectDocument()) {
            showStatus(getContext().getString(R.string.status_project_copy_failed),
                    R.attr.fsTextError);
        }
    }

    /**
     * Exports the current model as one GLB file.
     *
     * <p>The bytes are built BEFORE the picker opens, for the same reason Save
     * Copy does it: a model that cannot be exported should say so immediately
     * rather than after the user has chosen a destination and watched an empty
     * file appear there.
     *
     * <p>This reads the project and writes somewhere the user picked. It cannot
     * touch either {@code .forge} slot — neither the manual one nor the recovery
     * checkpoint — because it never calls anything that writes them.
     */
    @Override
    public void onExportGlbRequested() {
        final byte[] bytes = NativeViewport.exportGlb();
        if (bytes == null || bytes.length == 0) {
            // Null is every refusal at once — an empty scene, a mesh the writer
            // would not vouch for, a model past the size ceiling — and this
            // layer cannot tell them apart. So it says the one thing true of
            // all of them, rather than guessing "there is nothing to export"
            // over a model that is merely too large. The specific reason is in
            // the log, as FORGESHAPE_GLB_EXPORT_FAIL.
            Diagnostics.warn(DiagnosticLog.CAT_TRANSFER, "EXPORT_GLB_ENCODE_FAILED", null);
            showStatus(getContext().getString(R.string.status_export_glb_failed),
                    R.attr.fsTextError);
            return;
        }
        pendingGlbBytes = bytes;
        if (transferHost == null || !transferHost.requestCreateGlbDocument()) {
            pendingGlbBytes = null;
            showStatus(getContext().getString(R.string.status_export_glb_failed),
                    R.attr.fsTextError);
        }
    }

    /**
     * The bytes waiting for a destination.
     *
     * <p>Held between the request and the picker's answer, and cleared on every
     * outcome, so a model can never be written to a destination chosen for a
     * different action.
     */
    private byte[] pendingGlbBytes;

    /** Stages the bytes an export would write, without opening a picker. */
    void onExportGlbRequestedForTest(byte[] bytes) {
        pendingGlbBytes = bytes;
    }

    /** The user picked somewhere to put the exported model. */
    void onCreateGlbDocumentChosen(android.net.Uri destination) {
        final byte[] bytes = pendingGlbBytes;
        pendingGlbBytes = null;
        if (destination == null || bytes == null) {
            // Cancel. A no-op by construction: nothing was written, and the
            // export direction only ever read the project.
            Diagnostics.info(DiagnosticLog.CAT_TRANSFER, "EXPORT_GLB_CANCELLED", null);
            return;
        }
        if (!ProjectTransfer.writeTo(getContext(), destination, bytes)) {
            showStatus(getContext().getString(R.string.status_export_glb_failed),
                    R.attr.fsTextError);
            return;
        }
        Diagnostics.info(DiagnosticLog.CAT_TRANSFER, "EXPORT_GLB_WROTE",
                "bytes=" + bytes.length);
        showStatus(getContext().getString(R.string.status_export_glb_written, projectSummary()),
                R.attr.fsTextSuccess);
    }

    @Override
    public void onShareDiagnosticsRequested() {
        setProjectPanelOpen(false);
        // Written locally FIRST, so there is something real to hand over and so
        // the user could read it before deciding. Nothing is sent anywhere by
        // ForgeShape: the destination is whatever the system picker returns.
        if (!Diagnostics.writeReport(getContext(), Diagnostics.REASON_MANUAL)) {
            showStatus(getContext().getString(R.string.status_diagnostics_failed),
                    R.attr.fsTextError);
            return;
        }
        if (transferHost == null || !transferHost.requestCreateDiagnosticsDocument()) {
            showStatus(getContext().getString(R.string.status_diagnostics_failed),
                    R.attr.fsTextError);
        }
    }

    // -----------------------------------------------------------------------
    // IMPORT-01A — durable import
    // -----------------------------------------------------------------------
    //
    // The product path, and the only user-facing GLB route. It creates real
    // bodies: rows in the Objects list, ordinary gizmo targets, one Undo step
    // for the whole import, and geometry the project carries afterwards without
    // the source file. A refusal changes nothing at all — the domain builds and
    // validates every object before any of them reaches the scene — so there is
    // nothing to undo after one and no half-imported state to describe.

    @Override
    public void onImportGlbRequested() {
        setProjectPanelOpen(false);
        if (transferHost == null || !transferHost.requestOpenGlbDocument()) {
            showStatus(getContext().getString(R.string.status_glb_import_failed, "no picker"),
                    R.attr.fsTextError);
        }
    }

    /** The user picked a `.glb` to import — or cancelled. */
    void onOpenGlbDocumentChosen(android.net.Uri source) {
        if (source == null) {
            // Cancel. Nothing was read, nothing was created, nothing changed.
            Diagnostics.info(DiagnosticLog.CAT_TRANSFER, "GLB_IMPORT_CANCELLED", null);
            return;
        }
        final byte[] bytes = ProjectTransfer.readFrom(getContext(), source);
        if (bytes == null) {
            showStatus(getContext().getString(R.string.status_glb_import_failed, "unreadable"),
                    R.attr.fsTextError);
            return;
        }
        applyImportedGlbBytes(bytes);
    }

    /**
     * Imports bytes as durable objects.
     *
     * <p>Separate from the picker half so a test can drive the real parse and
     * the real commit without the system's document UI, which is another app's
     * surface and cannot be driven reliably from instrumentation. What is under
     * test — the parse, the refusal, the objects, the untouched project on a
     * refusal — is identical either way.
     */
    void applyImportedGlbBytes(byte[] bytes) {
        // Counted on this side because the count the user cares about is how
        // many objects APPEARED, which is a fact about the scene either side of
        // the call rather than about the file.
        final int before = NativeViewport.sceneBodyCount();
        final int status = NativeViewport.importGlbDurable(bytes);
        if (status != NativeViewport.IMPORT_OK) {
            // Fail closed and say so. The project is untouched by construction:
            // a refused file never reaches the scene, and a refused commit
            // never created an ObjectId.
            //
            // The user is shown one of three bounded categories, because those
            // are the three things a person can act on. The precise reason is a
            // stable token and goes to the diagnostics ring and the log, where
            // somebody chasing a particular file can read it.
            Diagnostics.warn(DiagnosticLog.CAT_TRANSFER, "GLB_IMPORT_REFUSED",
                    importRefusalToken(status));
            showStatus(getContext().getString(R.string.status_glb_import_failed,
                    getContext().getString(importRefusalCategory(status))),
                    R.attr.fsTextError);
            return;
        }
        Diagnostics.info(DiagnosticLog.CAT_TRANSFER, "GLB_IMPORTED", "bytes=" + bytes.length);
        showStatus(getContext().getString(R.string.status_glb_imported,
                importedSummary(NativeViewport.sceneBodyCount() - before)),
                R.attr.fsTextSuccess);
        // A real project change: the Objects list, the inspector and the
        // autosave fingerprint all have to see the new bodies.
        onNativeStateChanged();
    }

    /**
     * The stable refusal token, from whichever vocabulary the status belongs to.
     *
     * <p>Below {@link NativeViewport#IMPORT_COMMIT_BASE} the refusal is about
     * the FILE; at or above it, about the PROJECT. Keeping them apart is what
     * lets "this file uses a sparse accessor" and "this project cannot hold
     * that many bodies" stay two different, actionable sentences in the log.
     */
    private static String importRefusalToken(int status) {
        return status >= NativeViewport.IMPORT_COMMIT_BASE
                ? NativeViewport.glbCommitStatusToken(status)
                : NativeViewport.glbImportStatusToken(status);
    }

    /**
     * The bounded reason string for a refusal.
     *
     * <p>Three answers and no more: the file is not one this reader can open,
     * it uses features this import does not read, or its own geometry does not
     * add up. Which one is the domain's decision — the mapping lives beside the
     * status enums in C++, so the Android layer never has to know which refusal
     * means what.
     */
    private static int importRefusalCategory(int status) {
        final int category = status >= NativeViewport.IMPORT_COMMIT_BASE
                ? NativeViewport.glbCommitStatusCategory(status)
                : NativeViewport.glbImportStatusCategory(status);
        switch (category) {
            case NativeViewport.IMPORT_CATEGORY_UNSUPPORTED:
                return R.string.glb_refusal_unsupported;
            case NativeViewport.IMPORT_CATEGORY_INCONSISTENT:
                return R.string.glb_refusal_inconsistent;
            default:
                return R.string.glb_refusal_unreadable;
        }
    }

    /**
     * A short human summary of what arrived, for the status line.
     *
     * <p>One object names itself, because that is what the user is now looking
     * at and the name is the thing they will find in the list. Several are
     * counted, because reading out forty names is not a status line.
     */
    private String importedSummary(int created) {
        if (created == 1) {
            return NativeViewport.sceneBodyName(NativeViewport.sceneActiveBodyId());
        }
        return created + " objects";
    }

    /** True while the viewport is showing an imported file instead of the model. */
    boolean showingImportedPreview() {
        return NativeViewport.glbPreviewVisible();
    }

    /**
     * The bytes waiting for a destination.
     *
     * <p>Held here between the request and the picker's answer because the
     * picker is a whole activity round trip. Cleared on every outcome — success,
     * failure and cancel — so a project can never be written to a destination
     * chosen for a different one.
     */
    private byte[] pendingCopyBytes;

    /**
     * Stages the bytes a Save Copy would write, without opening a picker.
     *
     * <p>Verification only. The system's document UI belongs to another app,
     * differs per device and cannot be driven reliably from instrumentation, so
     * a test stages what the real request stages and then drives the real
     * result handler. What is under test — the bytes, the truncation, the
     * cancel, the untouched internal slot — is identical either way; what is
     * skipped is only the picker's own screen, and the Intent that asks for it
     * is asserted separately.
     */
    void onSaveCopyRequestedForTest(byte[] bytes) {
        pendingCopyBytes = bytes;
    }

    /** What the workspace needs from the Activity to reach the system picker. */
    interface ProjectTransferHost {
        boolean requestCreateProjectDocument();

        boolean requestOpenProjectDocument();

        boolean requestCreateDiagnosticsDocument();

        boolean requestCreateGlbDocument();

        boolean requestOpenGlbDocument();
    }

    private ProjectTransferHost transferHost;

    void setProjectTransferHost(ProjectTransferHost host) {
        this.transferHost = host;
    }

    /**
     * The user picked somewhere to put a copy of the project.
     *
     * <p>Writes the bytes captured when the action was requested, not a fresh
     * encode: the project may have changed while the picker was open, and the
     * copy the user asked for is the one they asked for.
     */
    void onCreateProjectDocumentChosen(android.net.Uri destination) {
        final byte[] bytes = pendingCopyBytes;
        pendingCopyBytes = null;
        if (destination == null || bytes == null) {
            // Cancel. A no-op by construction: nothing was written, the manual
            // slot was never involved, and the live project was only read.
            Diagnostics.info(DiagnosticLog.CAT_TRANSFER, "SAVE_COPY_CANCELLED", null);
            return;
        }
        if (!ProjectTransfer.writeTo(getContext(), destination, bytes)) {
            showStatus(getContext().getString(R.string.status_project_copy_failed),
                    R.attr.fsTextError);
            return;
        }
        Diagnostics.info(DiagnosticLog.CAT_TRANSFER, "SAVE_COPY_WROTE",
                "bytes=" + bytes.length);
        showStatus(getContext().getString(R.string.status_project_copy_saved, projectSummary()),
                R.attr.fsTextSuccess);
    }

    /**
     * The user picked a project file to open.
     *
     * <p>Reads bytes and hands them to the same fail-closed native load an
     * internal Open uses. <b>The internal manual slot is not written by this</b>
     * — opening a file makes that project live, and what the user has saved
     * stays what the user saved until they save again. Autosave protects the
     * newly live project from there, which is the difference between "this is
     * open" and "this is stored".
     */
    void onOpenProjectDocumentChosen(android.net.Uri source) {
        if (source == null) {
            Diagnostics.info(DiagnosticLog.CAT_TRANSFER, "OPEN_FILE_CANCELLED", null);
            return;
        }
        final byte[] bytes = ProjectTransfer.readFrom(getContext(), source);
        if (bytes == null) {
            showStatus(getContext().getString(R.string.status_project_damaged),
                    R.attr.fsTextError);
            return;
        }
        final int status = NativeViewport.loadProject(bytes);
        if (status != NativeViewport.PROJECT_OK) {
            Diagnostics.warn(DiagnosticLog.CAT_TRANSFER, "OPEN_FILE_REJECTED",
                    "status=" + status);
            showStatus(getContext().getString(projectFailureMessage(status)),
                    R.attr.fsTextError);
            return;
        }
        dismissPrimarySurfacesExcept(null);
        onNativeStateChanged();
        // The live project is now something that exists nowhere in this app's
        // own storage, so it is exactly the case autosave is for.
        noteProjectMaybeDirty();
        Diagnostics.info(DiagnosticLog.CAT_TRANSFER, "OPEN_FILE_APPLIED",
                "bodies=" + NativeViewport.sceneBodyCount());
        showStatus(getContext().getString(R.string.status_project_file_opened, projectSummary()),
                R.attr.fsTextSuccess);
    }

    /** The user picked somewhere to put the diagnostic report. */
    void onCreateDiagnosticsDocumentChosen(android.net.Uri destination) {
        if (destination == null) {
            return;
        }
        final byte[] report = Diagnostics.renderReport(getContext(), Diagnostics.REASON_MANUAL,
                null).getBytes(java.nio.charset.StandardCharsets.UTF_8);
        if (!ProjectTransfer.writeTo(getContext(), destination, report)) {
            showStatus(getContext().getString(R.string.status_diagnostics_failed),
                    R.attr.fsTextError);
            return;
        }
        showStatus(getContext().getString(R.string.status_diagnostics_shared),
                R.attr.fsTextPrimary);
    }

    // -----------------------------------------------------------------------
    // The renderer stopping
    // -----------------------------------------------------------------------

    /**
     * Reacts to the viewport having stopped for good.
     *
     * <p>GPU resources were never project truth, so the work is intact in CPU
     * domain state — and the first thing to do about that is get it onto disk,
     * because a process whose renderer has died is a process that may not last.
     * Then say so: a black viewport with no explanation is worse than a sentence
     * that names the one thing that fixes it.
     */
    void onRendererRestartRequired() {
        requestImmediateCheckpoint();
        Diagnostics.error(DiagnosticLog.CAT_RENDER, "RESTART_REQUIRED",
                "bodies=" + NativeViewport.sceneBodyCount());
        Diagnostics.writeReport(getContext(), Diagnostics.REASON_RENDER_RESTART_REQUIRED);
        showStatus(getContext().getString(R.string.status_renderer_restart_required),
                R.attr.fsTextError);
    }

    private void setProjectPanelOpen(boolean open) {
        if (projectPopover.isOpen() == open) {
            return;
        }
        projectPopover.setOpen(open);
        toolbar.showProjectActionsOpen(open);
    }

    /** Which failure the user is looking at. One message per honest cause. */
    private static int projectFailureMessage(int status) {
        switch (status) {
            case NativeViewport.PROJECT_NO_DATA:
                return R.string.status_project_none;
            case NativeViewport.PROJECT_NOT_A_PROJECT:
                return R.string.status_project_not_a_project;
            case NativeViewport.PROJECT_UNSUPPORTED_VERSION:
                return R.string.status_project_unsupported;
            case NativeViewport.PROJECT_INVALID:
                return R.string.status_project_invalid;
            case NativeViewport.PROJECT_BUSY:
                return R.string.status_project_busy;
            case NativeViewport.PROJECT_DAMAGED:
            default:
                return R.string.status_project_damaged;
        }
    }

    /**
     * What the project is, in the two facts a user checks after a save or an
     * open: how many bodies, and which representation they are working in.
     *
     * <p>Read from native truth rather than from anything the chrome is
     * currently showing, so the sentence describes the model and not the panel.
     */
    private String projectSummary() {
        final int bodies = NativeViewport.sceneBodyCount();
        final String count = getResources().getQuantityString(
                R.plurals.project_body_count, bodies, bodies);
        return getContext().getString(isSculpting() ? R.string.project_summary_sculpt
                                                    : R.string.project_summary_construction,
                count);
    }

    /**
     * One primary contextual surface at a time.
     *
     * <p>This names the five task surfaces explicitly rather than blindly
     * closing every anchored popover in the workspace. A future lightweight
     * popover that is proven collision-free is not silently pulled into this
     * policy just because it shares the anchored-surface motion primitive.
     * Each close uses the same path as its invoking control so focus, keyboard
     * ownership and active styling cannot drift.
     */
    private void dismissPrimarySurfacesExcept(AnchoredSurfaceView keeper) {
        if (keeper != inspector && inspector.isOpen()) {
            setPrecisionOpen(false);
        }
        if (keeper != objectsPopover && objectsPopover.isOpen()) {
            setObjectsPanelOpen(false);
        }
        if (keeper != addPrimitivePalette && addPrimitivePalette.isOpen()) {
            setAddPrimitiveOpen(false, null);
        }
        if (keeper != displayPopover && displayPopover.isOpen()) {
            displayPopover.setOpen(false);
            toolbar.showDisplaySettingsOpen(false);
        }
        if (keeper != projectPopover && projectPopover.isOpen()) {
            projectPopover.setOpen(false);
            toolbar.showProjectActionsOpen(false);
        }
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
        // Keep the sketch-mode tracker current on every chrome rebuild, so the
        // viewport-settle transition check only fires for a sketch that STARTED
        // in native (the spatial chooser's confirm tap) without a rebuild.
        lastKnownSketching = isSketching();
        // THE central re-read, and therefore the central place to notice that
        // the project may have moved. Every chrome-driven mutation — an Apply,
        // a creation, an undo, a mode change, a body selection — ends here, so
        // one note covers all of them rather than each call site remembering.
        // Selection counts: which body is active is part of the document.
        noteProjectMaybeDirty();
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
        // Rebased for exactly the same reason, and it matters more here: the
        // sculpt undo depth is PER BODY, so a body switch, a Back, a Resume or
        // a Reset from source all change the number this compares against
        // without any stroke having happened. Recording it wherever the chrome
        // re-reads keeps the comparison meaning "has a stroke committed since
        // these controls were last refreshed" rather than "is this a different
        // body's depth".
        lastKnownSculptUndoDepth = NativeViewport.sculptUndoDepth();
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
        allConverted &= sketchEditor.convertDisplayUnit(previous, unit);
        allConverted &= cadEditor.convertDisplayUnit(previous, unit);
        showStatus(getContext().getString(allConverted ? R.string.status_unit_display_only
                        : R.string.status_unit_unparsed, unit.label()),
                allConverted ? R.attr.fsTextSecondary : R.attr.fsTextError);
    }

    @Override
    public void finishEditing() {
        shapeEditor.clearEditFocus();
        placementEditor.clearEditFocus();
        sketchEditor.clearEditFocus();
        cadEditor.clearEditFocus();
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

    /**
     * The row the trailing host and a side-placed precision surface share.
     *
     * <p>Exposed so a case can assert their ORDER rather than their pixels: the
     * host keeps the trailing edge because it is the last child of this row, and
     * a panel that is appended after it instead of seated before it translates
     * the host by its own width. That is a composition fact, not a measurement,
     * and it is the one that holds in windows the harness cannot materialise.
     */
    LinearLayout workspaceMiddleRow() {
        return middleRow;
    }

    SculptContextView sculptContext() {
        return sculptContext;
    }

    ConstructionShapeEditorView shapeEditor() {
        return shapeEditor;
    }

    /** The placement editor, so a test can read a field's complete value rather
     *  than the shortened form the panel may be drawing. */
    ConstructionPlacementEditorView placementEditor() {
        return placementEditor;
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

    /** The Display popover, so a test can name it by more than its id. */
    DisplaySettingsPopoverView displayPopover() {
        return displayPopover;
    }

    /** The project surface, for verification that names it by semantic id. */
    ProjectActionsPopoverView projectPopover() {
        return projectPopover;
    }

    /**
     * Every surface that grows out of a control, in one place.
     *
     * <p>So a case can assert the shared motion contract over the SET rather
     * than over a list it maintains by hand — which is how the Display popover
     * came to be the only one with a correct first-open pivot: nothing was
     * measuring them together. The set has caught it twice now; the project
     * surface joined at E2E-R1A growing from the wrong corner, and UIR4B-08's
     * count is what made that a failure rather than a surprise later.
     */
    AnchoredSurfaceView[] anchoredSurfaces() {
        return new AnchoredSurfaceView[]{
                objectsPopover, addPrimitivePalette, inspector, displayPopover, projectPopover};
    }

    /** The direct brush controls, so a test can read the values beside them. */
    BrushEdgeControlsView brushControls() {
        return brushControls;
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

    /** Answers the start question the way a test that is not about it needs. */
    void dismissStartChooserForConstruction() {
        uiState.recordStartChoice();
        showStartChooser(false);
    }

    /**
     * How much the chrome is currently inset from the bottom of the window.
     *
     * <p>For verification. This is where the keyboard arrives: the IME is a
     * bottom inset the chrome consumes as padding and the {@code SurfaceView}
     * ignores, so a case about what a squeezed column does needs to be able to
     * prove the squeeze was actually in force when it measured.
     */
    int chromeBottomInsetPx() {
        return chromeRoot.getPaddingBottom();
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
        final View[] trailing = trailingHost.occludingSurfaces();
        final View[] surfaces = new View[trailing.length + 5];
        surfaces[0] = brushControls;
        System.arraycopy(trailing, 0, surfaces, 1, trailing.length);
        surfaces[trailing.length + 1] = objectsCapsule;
        surfaces[trailing.length + 2] = historyGroup;
        surfaces[trailing.length + 3] = inspector;
        surfaces[trailing.length + 4] = objectsDock;

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
