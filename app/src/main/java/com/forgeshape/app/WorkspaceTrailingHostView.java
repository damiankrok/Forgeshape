package com.forgeshape.app;

import android.content.Context;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;

/**
 * Owns composition and presentation of the Editor Workspace's right context.
 *
 * <p>The host is the one externally visible right surface. Its top, trailing
 * edge and width are fixed by this class; tool context may only change the
 * vertical content and therefore the bottom edge. When the window is short the
 * same vertical surface scrolls internally instead of inventing a horizontal or
 * detached control arrangement.
 *
 * <p>This view renders state supplied by {@link EditorWorkspaceView}; it owns no
 * native or product truth and executes no command. Its callbacks name user acts,
 * while the workspace remains the sole native/action-routing owner.
 */
final class WorkspaceTrailingHostView extends FrameLayout
        implements ToolRailView.OnToolSelected {

    interface Callbacks {
        void onToolSelected(int key);

        void onPrecisionRequested();

        void onTransformModeRequested(int mode);

        void onTransformSpaceRequested(int space);

        /** Drop the sketch in progress. Never a project mutation. */
        void onCancelSketchRequested();

        /** From the profile choice back to editing the sketch. */
        void onBackToSketchRequested();
    }

    /** One derived presentation snapshot; none of these values is product truth. */
    static final class PresentationState {
        final boolean sculpting;
        final int activeTool;
        final boolean transformOffered;
        final int transformMode;
        final boolean transformSpaceOffered;
        final int transformSpace;
        final boolean precisionOpen;
        final CharSequence precisionSurfaceName;
        final boolean compactRail;
        /** Whether the rail offers Shape. False for an Imported Mesh — see ensureEntries. */
        final boolean shapeOffered;
        final boolean displaySuppressed;
        /** The native sketch session's state; the rail carries the sketch tools
         *  while it is not inactive, and the sketch group under them. */
        final int sketchState;

        PresentationState(boolean sculpting, int activeTool,
                          boolean transformOffered, int transformMode,
                          boolean transformSpaceOffered, int transformSpace,
                          boolean precisionOpen, CharSequence precisionSurfaceName,
                          boolean compactRail, boolean displaySuppressed,
                          boolean shapeOffered) {
            this(sculpting, activeTool, transformOffered, transformMode, transformSpaceOffered,
                    transformSpace, precisionOpen, precisionSurfaceName, compactRail,
                    displaySuppressed, shapeOffered, NativeViewport.SKETCH_INACTIVE);
        }

        PresentationState(boolean sculpting, int activeTool,
                          boolean transformOffered, int transformMode,
                          boolean transformSpaceOffered, int transformSpace,
                          boolean precisionOpen, CharSequence precisionSurfaceName,
                          boolean compactRail, boolean displaySuppressed,
                          boolean shapeOffered, int sketchState) {
            this.sketchState = sketchState;
            this.sculpting = sculpting;
            this.activeTool = activeTool;
            this.transformOffered = transformOffered;
            this.transformMode = transformMode;
            this.transformSpaceOffered = transformSpaceOffered;
            this.transformSpace = transformSpace;
            this.precisionOpen = precisionOpen;
            this.precisionSurfaceName = precisionSurfaceName;
            this.compactRail = compactRail;
            this.displaySuppressed = displaySuppressed;
            this.shapeOffered = shapeOffered;
        }
    }

    private final Callbacks callbacks;
    private final BoundedScrollView contentScroll;
    private final LinearLayout contentColumn;
    private final ToolRailView toolRail;
    private final LinearLayout precisionGroup;
    private final ImageView precisionToggle;
    private final LinearLayout transformSelectorRow;
    private final LinearLayout transformModeGroup;
    private final ImageView transformMoveAction;
    private final ImageView transformRotateAction;
    private final ImageView transformScaleAction;
    private final LinearLayout transformSpaceGroup;
    private final ImageView transformSpaceWorldAction;
    private final ImageView transformSpaceLocalAction;
    /** The sketch's own group: Cancel Sketch, and Back to Sketch once finished. */
    private final LinearLayout sketchGroup;
    private final ImageView cancelSketchAction;
    private final ImageView backToSketchAction;

    private Boolean showingSculptEntries;
    /** Whether the rail last drew the sketch tools. Null until the first render. */
    private Boolean showingSketchEntries;
    /** Whether the rail last drew a Shape entry. Null until the first render. */
    private Boolean showingShapeEntry;
    private int upstreamToolbarExpansionPx;

    WorkspaceTrailingHostView(Context context, Callbacks callbacks) {
        super(context);
        this.callbacks = callbacks;
        setId(R.id.workspace_trailing_host);
        setClickable(true);
        EditorControlStyles.applyFloatingSurface(this);
        EditorControlStyles.allowChildShadows(this);

        contentScroll = new BoundedScrollView(context);
        contentScroll.setId(R.id.tool_rail_scroll);
        contentScroll.setFillViewport(false);
        contentScroll.setVerticalScrollBarEnabled(false);

        contentColumn = new LinearLayout(context);
        contentColumn.setOrientation(LinearLayout.VERTICAL);
        contentColumn.setGravity(Gravity.CENTER_HORIZONTAL);
        final int hostPadding = EditorControlStyles.dimen(context, R.dimen.rail_padding);
        contentColumn.setPadding(hostPadding, hostPadding, hostPadding, hostPadding);
        contentScroll.addView(contentColumn, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        addView(contentScroll, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        toolRail = new ToolRailView(context, this);
        contentColumn.addView(toolRail, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        transformModeGroup = internalGroup(context, R.id.transform_mode_group);
        transformMoveAction = selectorButton(context, R.id.transform_mode_move,
                R.drawable.ic_gizmo_move, R.string.transform_mode_move,
                view -> callbacks.onTransformModeRequested(NativeViewport.GIZMO_MODE_MOVE));
        transformModeGroup.addView(transformMoveAction, internalButtonParams(context, 0));
        transformRotateAction = selectorButton(context, R.id.transform_mode_rotate,
                R.drawable.ic_gizmo_rotate, R.string.transform_mode_rotate,
                view -> callbacks.onTransformModeRequested(NativeViewport.GIZMO_MODE_ROTATE));
        transformModeGroup.addView(transformRotateAction, internalButtonParams(context,
                EditorControlStyles.dimen(context, R.dimen.rail_item_gap)));
        transformScaleAction = selectorButton(context, R.id.transform_mode_scale,
                R.drawable.ic_gizmo_scale, R.string.transform_mode_scale,
                view -> callbacks.onTransformModeRequested(NativeViewport.GIZMO_MODE_SCALE));
        transformModeGroup.addView(transformScaleAction, internalButtonParams(context,
                EditorControlStyles.dimen(context, R.dimen.rail_item_gap)));

        transformSpaceGroup = internalGroup(context, R.id.transform_space_group);
        transformSpaceWorldAction = selectorButton(context, R.id.transform_space_world,
                R.drawable.ic_space_world, R.string.transform_space_world,
                view -> callbacks.onTransformSpaceRequested(NativeViewport.GIZMO_SPACE_WORLD));
        transformSpaceGroup.addView(transformSpaceWorldAction, internalButtonParams(context, 0));
        transformSpaceLocalAction = selectorButton(context, R.id.transform_space_local,
                R.drawable.ic_space_local, R.string.transform_space_local,
                view -> callbacks.onTransformSpaceRequested(NativeViewport.GIZMO_SPACE_LOCAL));
        transformSpaceGroup.addView(transformSpaceLocalAction, internalButtonParams(context,
                EditorControlStyles.dimen(context, R.dimen.rail_item_gap)));

        transformSelectorRow = new LinearLayout(context);
        transformSelectorRow.setId(R.id.transform_selector_row);
        transformSelectorRow.setOrientation(LinearLayout.VERTICAL);
        transformSelectorRow.setGravity(Gravity.CENTER_HORIZONTAL);
        transformSelectorRow.setVisibility(GONE);
        transformSelectorRow.addView(transformModeGroup, sectionParams(context, 0));
        transformSelectorRow.addView(transformSpaceGroup, sectionParams(context,
                EditorControlStyles.dimen(context, R.dimen.row_gap_small)));
        contentColumn.addView(transformSelectorRow, sectionParams(context,
                EditorControlStyles.dimen(context, R.dimen.row_gap_small)));

        // The sketch group, under the sketch tools and above the precision
        // toggle, on the transform selector's terms: a vertical group inside
        // the one trailing surface, present only while a sketch is open. Cancel
        // is the way out that keeps nothing; Back to Sketch returns from the
        // profile choice to drawing. Both carry their full wording as content
        // descriptions.
        sketchGroup = internalGroup(context, R.id.sketch_group);
        cancelSketchAction = selectorButton(context, R.id.cancel_sketch,
                R.drawable.ic_sketch_cancel, R.string.cancel_sketch,
                view -> callbacks.onCancelSketchRequested());
        sketchGroup.addView(cancelSketchAction, internalButtonParams(context, 0));
        backToSketchAction = selectorButton(context, R.id.back_to_sketch,
                R.drawable.ic_sketch_back, R.string.back_to_sketch,
                view -> callbacks.onBackToSketchRequested());
        sketchGroup.addView(backToSketchAction, internalButtonParams(context,
                EditorControlStyles.dimen(context, R.dimen.rail_item_gap)));
        sketchGroup.setVisibility(GONE);
        contentColumn.addView(sketchGroup, sectionParams(context,
                EditorControlStyles.dimen(context, R.dimen.row_gap_small)));

        precisionGroup = internalGroup(context, R.id.precision_group);
        precisionToggle = EditorControlStyles.iconButton(context, R.id.precision_toggle,
                R.drawable.ic_precision, context.getString(R.string.precision_shape));
        precisionToggle.setBackgroundResource(R.drawable.bg_rail_entry);
        precisionToggle.setOnClickListener(view -> callbacks.onPrecisionRequested());
        precisionGroup.addView(precisionToggle, internalButtonParams(context, 0));
        contentColumn.addView(precisionGroup, sectionParams(context,
                EditorControlStyles.dimen(context, R.dimen.row_gap_small)));
    }

    /** The fixed external geometry this host presents to the workspace row. */
    LinearLayout.LayoutParams parentLayoutParams() {
        final LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                EditorControlStyles.dimen(getContext(), R.dimen.trailing_host_width),
                ViewGroup.LayoutParams.WRAP_CONTENT);
        applyParentPlacement(params);
        return params;
    }

    /** Re-resolves density-backed margins and width after configuration changes. */
    void refreshParentPlacement() {
        final ViewGroup.LayoutParams params = getLayoutParams();
        if (params instanceof LinearLayout.LayoutParams) {
            final LinearLayout.LayoutParams typed = (LinearLayout.LayoutParams) params;
            typed.width = EditorControlStyles.dimen(getContext(), R.dimen.trailing_host_width);
            applyParentPlacement(typed);
            setLayoutParams(typed);
        }
    }

    /**
     * Keeps the external top independent of an unrelated second toolbar line.
     *
     * <p>The root owns Global Toolbar measurement and supplies only the extra
     * height above its fixed controls row. Applying the compensation here keeps
     * right-host geometry with its composition owner; it changes no product or
     * inset authority and is intentionally not animated.
     */
    boolean setUpstreamToolbarExpansionPx(int expansionPx) {
        final int resolved = Math.max(0, expansionPx);
        if (upstreamToolbarExpansionPx == resolved) {
            return false;
        }
        upstreamToolbarExpansionPx = resolved;
        final ViewGroup.LayoutParams params = getLayoutParams();
        if (params instanceof LinearLayout.LayoutParams) {
            applyParentPlacement((LinearLayout.LayoutParams) params);
        }
        return true;
    }

    private void applyParentPlacement(LinearLayout.LayoutParams params) {
        params.gravity = Gravity.TOP;
        params.topMargin = EditorControlStyles.dimen(getContext(), R.dimen.row_gap)
                - upstreamToolbarExpansionPx;
        params.rightMargin = EditorControlStyles.dimen(getContext(), R.dimen.brush_gap);
    }

    void render(PresentationState state) {
        final boolean sketching = state.sketchState != NativeViewport.SKETCH_INACTIVE;
        ensureEntries(state.sculpting, state.shapeOffered, sketching);
        toolRail.setCompactEntries(state.compactRail);
        toolRail.showActive(state.activeTool);
        sketchGroup.setVisibility(sketching ? VISIBLE : GONE);
        backToSketchAction.setVisibility(
                state.sketchState == NativeViewport.SKETCH_READY ? VISIBLE : GONE);

        precisionToggle.setContentDescription(getContext().getString(
                state.precisionOpen ? R.string.precision_close : R.string.precision_open,
                state.precisionSurfaceName));
        setInternalButtonActive(precisionToggle, state.precisionOpen);

        transformSelectorRow.setVisibility(state.transformOffered ? VISIBLE : GONE);
        transformModeGroup.setVisibility(state.transformOffered ? VISIBLE : GONE);
        transformSpaceGroup.setVisibility(state.transformSpaceOffered ? VISIBLE : GONE);
        if (state.transformOffered) {
            setInternalButtonActive(transformMoveAction,
                    state.transformMode == NativeViewport.GIZMO_MODE_MOVE);
            setInternalButtonActive(transformRotateAction,
                    state.transformMode == NativeViewport.GIZMO_MODE_ROTATE);
            setInternalButtonActive(transformScaleAction,
                    state.transformMode == NativeViewport.GIZMO_MODE_SCALE);
        }
        if (state.transformSpaceOffered) {
            setInternalButtonActive(transformSpaceWorldAction,
                    state.transformSpace == NativeViewport.GIZMO_SPACE_WORLD);
            setInternalButtonActive(transformSpaceLocalAction,
                    state.transformSpace == NativeViewport.GIZMO_SPACE_LOCAL);
        }
        setVisibility(state.displaySuppressed ? GONE : VISIBLE);
    }

    /** The one external surface contributed to viewport-occlusion accounting. */
    View[] occludingSurfaces() {
        return new View[]{this};
    }

    @Override
    public void onToolSelected(int key) {
        callbacks.onToolSelected(key);
    }

    private ImageView selectorButton(Context context, int id, int icon, int label,
                                     OnClickListener listener) {
        final ImageView action = EditorControlStyles.iconButton(context, id, icon,
                context.getString(label));
        action.setBackgroundResource(R.drawable.bg_rail_entry);
        action.setOnClickListener(listener);
        return action;
    }

    private static LinearLayout internalGroup(Context context, int id) {
        final LinearLayout group = new LinearLayout(context);
        group.setId(id);
        group.setOrientation(LinearLayout.VERTICAL);
        group.setGravity(Gravity.CENTER_HORIZONTAL);
        return group;
    }

    private static LinearLayout.LayoutParams internalButtonParams(Context context,
                                                                   int topMargin) {
        final LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                EditorControlStyles.dimen(context, R.dimen.rail_item_width),
                EditorControlStyles.dimen(context, R.dimen.control_height));
        params.topMargin = topMargin;
        return params;
    }

    private static LinearLayout.LayoutParams sectionParams(Context context, int topMargin) {
        final LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.gravity = Gravity.CENTER_HORIZONTAL;
        params.topMargin = topMargin;
        return params;
    }

    private static void setInternalButtonActive(ImageView button, boolean active) {
        button.setBackgroundResource(active
                ? R.drawable.bg_rail_entry_active : R.drawable.bg_rail_entry);
        button.setActivated(active);
    }

    private void ensureEntries(boolean sculpting, boolean shapeOffered, boolean sketching) {
        if (showingSculptEntries != null && showingSculptEntries == sculpting
                && showingShapeEntry != null && showingShapeEntry == shapeOffered
                && showingSketchEntries != null && showingSketchEntries == sketching
                && toolRail.getChildCount() > 0) {
            return;
        }
        showingSculptEntries = sculpting;
        showingShapeEntry = shapeOffered;
        showingSketchEntries = sketching;
        final Context context = getContext();
        if (sketching) {
            // The seven sketch tools (CAD-R0-A1A2, plus Arc and Spline from
            // `SKETCH-UX-R1` D). Select is a tool so that a tap in the viewport
            // has exactly one meaning at a time. Arc and Spline are APPENDED, so
            // the five that were here keep their order and their indices.
            toolRail.setEntries(new ToolRailView.Entry[]{
                    new ToolRailView.Entry(R.id.tool_rail_select, R.drawable.ic_tool_select,
                            context.getString(R.string.tool_select),
                            NativeViewport.SKETCH_TOOL_SELECT),
                    new ToolRailView.Entry(R.id.tool_rail_line, R.drawable.ic_tool_line,
                            context.getString(R.string.tool_line), NativeViewport.SKETCH_TOOL_LINE),
                    new ToolRailView.Entry(R.id.tool_rail_polyline, R.drawable.ic_tool_polyline,
                            context.getString(R.string.tool_polyline),
                            NativeViewport.SKETCH_TOOL_POLYLINE),
                    new ToolRailView.Entry(R.id.tool_rail_rectangle,
                            R.drawable.ic_tool_rectangle,
                            context.getString(R.string.tool_rectangle),
                            NativeViewport.SKETCH_TOOL_RECTANGLE),
                    new ToolRailView.Entry(R.id.tool_rail_circle, R.drawable.ic_tool_circle,
                            context.getString(R.string.tool_circle),
                            NativeViewport.SKETCH_TOOL_CIRCLE),
                    new ToolRailView.Entry(R.id.tool_rail_arc, R.drawable.ic_tool_arc,
                            context.getString(R.string.tool_arc), NativeViewport.SKETCH_TOOL_ARC),
                    new ToolRailView.Entry(R.id.tool_rail_spline, R.drawable.ic_tool_spline,
                            context.getString(R.string.tool_spline),
                            NativeViewport.SKETCH_TOOL_SPLINE),
            });
            return;
        }
        if (sculpting) {
            toolRail.setEntries(new ToolRailView.Entry[]{
                    new ToolRailView.Entry(R.id.tool_rail_grab, R.drawable.ic_tool_grab,
                            context.getString(R.string.tool_grab), NativeViewport.TOOL_GRAB),
                    new ToolRailView.Entry(R.id.tool_rail_clay, R.drawable.ic_tool_clay,
                            context.getString(R.string.tool_clay), NativeViewport.TOOL_CLAY),
                    new ToolRailView.Entry(R.id.tool_rail_smooth, R.drawable.ic_tool_smooth,
                            context.getString(R.string.tool_smooth), NativeViewport.TOOL_SMOOTH),
                    new ToolRailView.Entry(R.id.tool_rail_inflate, R.drawable.ic_tool_inflate,
                            context.getString(R.string.tool_inflate),
                            NativeViewport.TOOL_INFLATE),
            });
            return;
        }
        final ToolRailView.Entry place = new ToolRailView.Entry(R.id.tool_rail_place,
                R.drawable.ic_tool_place, context.getString(R.string.tool_transform),
                EditorUiState.CONSTRUCTION_TOOL_TRANSFORM);
        if (!shapeOffered) {
            // An Imported Mesh has no primitive and no dimensions, so Shape has
            // nothing to edit and the domain refuses a shape Apply for one. The
            // entry is ABSENT rather than drawn and inert: a control that
            // cannot succeed is not drawn. Transform stays, because an imported
            // body's placement is an ordinary ForgeShape placement.
            toolRail.setEntries(new ToolRailView.Entry[]{place});
            return;
        }
        toolRail.setEntries(new ToolRailView.Entry[]{
                new ToolRailView.Entry(R.id.tool_rail_shape, R.drawable.ic_tool_shape,
                        context.getString(R.string.tool_shape),
                        EditorUiState.CONSTRUCTION_TOOL_SHAPE),
                place,
        });
    }

    /** The unified surface consumes gaps between its internal controls. */
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return true;
    }
}
