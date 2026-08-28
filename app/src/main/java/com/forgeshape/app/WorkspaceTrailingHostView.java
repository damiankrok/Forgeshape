package com.forgeshape.app;

import android.content.Context;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;

/**
 * Owns composition and presentation of the Editor Workspace's trailing cluster.
 *
 * <p>This view renders state supplied by {@link EditorWorkspaceView}; it owns no
 * native or product truth and executes no command. Its callbacks name user acts,
 * while the workspace remains the sole native/action-routing owner.
 */
final class WorkspaceTrailingHostView extends TrailingClusterColumn
        implements ToolRailView.OnToolSelected {

    interface Callbacks {
        void onToolSelected(int key);

        void onPrecisionRequested();

        void onTransformModeRequested(int mode);

        void onTransformSpaceRequested(int space);
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
        final boolean compactSelectors;
        final boolean displaySuppressed;

        PresentationState(boolean sculpting, int activeTool,
                          boolean transformOffered, int transformMode,
                          boolean transformSpaceOffered, int transformSpace,
                          boolean precisionOpen, CharSequence precisionSurfaceName,
                          boolean compactRail, boolean compactSelectors,
                          boolean displaySuppressed) {
            this.sculpting = sculpting;
            this.activeTool = activeTool;
            this.transformOffered = transformOffered;
            this.transformMode = transformMode;
            this.transformSpaceOffered = transformSpaceOffered;
            this.transformSpace = transformSpace;
            this.precisionOpen = precisionOpen;
            this.precisionSurfaceName = precisionSurfaceName;
            this.compactRail = compactRail;
            this.compactSelectors = compactSelectors;
            this.displaySuppressed = displaySuppressed;
        }
    }

    private final Callbacks callbacks;
    private final ToolRailView toolRail;
    private final BoundedScrollView toolRailScroll;
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

    private Boolean showingSculptEntries;

    WorkspaceTrailingHostView(Context context, Callbacks callbacks) {
        super(context);
        this.callbacks = callbacks;
        setId(R.id.workspace_trailing_host);
        setGravity(Gravity.END);
        EditorControlStyles.allowChildShadows(this);

        toolRail = new ToolRailView(context, this);
        toolRailScroll = new BoundedScrollView(context);
        toolRailScroll.setId(R.id.tool_rail_scroll);
        EditorControlStyles.applyFloatingSurface(toolRailScroll);
        toolRailScroll.addView(toolRail, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        addView(toolRailScroll, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        setFlexibleChild(toolRailScroll, 0);

        precisionGroup = EditorControlStyles.controlGroup(context);
        precisionGroup.setId(R.id.precision_group);
        precisionToggle = EditorControlStyles.iconButton(context, R.id.precision_toggle,
                R.drawable.ic_precision, context.getString(R.string.precision_shape));
        precisionToggle.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View view) {
                callbacks.onPrecisionRequested();
            }
        });
        precisionGroup.addView(precisionToggle,
                EditorControlStyles.iconButtonParams(context, 0));
        final LinearLayout.LayoutParams precisionParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        precisionParams.gravity = Gravity.END;
        precisionParams.topMargin = EditorControlStyles.dimen(context, R.dimen.row_gap_small);
        addView(precisionGroup, precisionParams);

        transformModeGroup = EditorControlStyles.controlGroup(context);
        transformModeGroup.setId(R.id.transform_mode_group);
        transformModeGroup.setOrientation(LinearLayout.VERTICAL);
        transformModeGroup.setGravity(Gravity.CENTER_HORIZONTAL);
        transformMoveAction = selectorButton(context, R.id.transform_mode_move,
                R.drawable.ic_gizmo_move, R.string.transform_mode_move,
                new OnClickListener() {
                    @Override
                    public void onClick(View view) {
                        callbacks.onTransformModeRequested(NativeViewport.GIZMO_MODE_MOVE);
                    }
                });
        transformModeGroup.addView(transformMoveAction,
                EditorControlStyles.iconButtonParams(context, 0));
        transformRotateAction = selectorButton(context, R.id.transform_mode_rotate,
                R.drawable.ic_gizmo_rotate, R.string.transform_mode_rotate,
                new OnClickListener() {
                    @Override
                    public void onClick(View view) {
                        callbacks.onTransformModeRequested(NativeViewport.GIZMO_MODE_ROTATE);
                    }
                });
        transformModeGroup.addView(transformRotateAction, selectorFollowerParams(context));
        transformScaleAction = selectorButton(context, R.id.transform_mode_scale,
                R.drawable.ic_gizmo_scale, R.string.transform_mode_scale,
                new OnClickListener() {
                    @Override
                    public void onClick(View view) {
                        callbacks.onTransformModeRequested(NativeViewport.GIZMO_MODE_SCALE);
                    }
                });
        transformModeGroup.addView(transformScaleAction, selectorFollowerParams(context));
        transformModeGroup.setVisibility(GONE);

        transformSelectorRow = new LinearLayout(context);
        transformSelectorRow.setId(R.id.transform_selector_row);
        transformSelectorRow.setOrientation(LinearLayout.VERTICAL);
        transformSelectorRow.setGravity(Gravity.END);
        EditorControlStyles.allowChildShadows(transformSelectorRow);
        transformSelectorRow.setVisibility(GONE);
        transformSelectorRow.addView(transformModeGroup, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        transformSpaceGroup = EditorControlStyles.controlGroup(context);
        transformSpaceGroup.setId(R.id.transform_space_group);
        transformSpaceGroup.setOrientation(LinearLayout.VERTICAL);
        transformSpaceGroup.setGravity(Gravity.CENTER_HORIZONTAL);
        transformSpaceWorldAction = selectorButton(context, R.id.transform_space_world,
                R.drawable.ic_space_world, R.string.transform_space_world,
                new OnClickListener() {
                    @Override
                    public void onClick(View view) {
                        callbacks.onTransformSpaceRequested(NativeViewport.GIZMO_SPACE_WORLD);
                    }
                });
        transformSpaceGroup.addView(transformSpaceWorldAction,
                EditorControlStyles.iconButtonParams(context, 0));
        transformSpaceLocalAction = selectorButton(context, R.id.transform_space_local,
                R.drawable.ic_space_local, R.string.transform_space_local,
                new OnClickListener() {
                    @Override
                    public void onClick(View view) {
                        callbacks.onTransformSpaceRequested(NativeViewport.GIZMO_SPACE_LOCAL);
                    }
                });
        transformSpaceGroup.addView(transformSpaceLocalAction, selectorFollowerParams(context));
        transformSpaceGroup.setVisibility(GONE);

        final LinearLayout.LayoutParams transformSpaceParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        transformSpaceParams.gravity = Gravity.END;
        transformSpaceParams.topMargin =
                EditorControlStyles.dimen(context, R.dimen.row_gap_small);
        transformSelectorRow.addView(transformSpaceGroup, transformSpaceParams);

        final LinearLayout.LayoutParams transformRowParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        transformRowParams.gravity = Gravity.END;
        transformRowParams.topMargin =
                EditorControlStyles.dimen(context, R.dimen.row_gap_small);
        addView(transformSelectorRow, transformRowParams);
    }

    /** The unchanged top/right contract this host presents to the workspace row. */
    LinearLayout.LayoutParams parentLayoutParams() {
        final LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        applyParentPlacement(params);
        return params;
    }

    /** Re-resolves density-backed margins when the window configuration changes. */
    void refreshParentPlacement() {
        final ViewGroup.LayoutParams params = getLayoutParams();
        if (params instanceof LinearLayout.LayoutParams) {
            applyParentPlacement((LinearLayout.LayoutParams) params);
            setLayoutParams(params);
        }
    }

    private void applyParentPlacement(LinearLayout.LayoutParams params) {
        params.gravity = Gravity.TOP;
        params.topMargin = EditorControlStyles.dimen(getContext(), R.dimen.row_gap);
        params.rightMargin = EditorControlStyles.dimen(getContext(), R.dimen.brush_gap);
    }

    void render(PresentationState state) {
        ensureEntries(state.sculpting);
        toolRail.setCompactEntries(state.compactRail);
        toolRail.showActive(state.activeTool);

        precisionToggle.setContentDescription(getContext().getString(
                state.precisionOpen ? R.string.precision_close : R.string.precision_open,
                state.precisionSurfaceName));
        EditorControlStyles.setIconButtonActive(precisionToggle, state.precisionOpen);

        applySelectorOrientation(state.compactSelectors);
        transformSelectorRow.setVisibility(state.transformOffered ? VISIBLE : GONE);
        transformModeGroup.setVisibility(state.transformOffered ? VISIBLE : GONE);
        transformSpaceGroup.setVisibility(state.transformSpaceOffered ? VISIBLE : GONE);
        if (state.transformOffered) {
            EditorControlStyles.setIconButtonActive(transformMoveAction,
                    state.transformMode == NativeViewport.GIZMO_MODE_MOVE);
            EditorControlStyles.setIconButtonActive(transformRotateAction,
                    state.transformMode == NativeViewport.GIZMO_MODE_ROTATE);
            EditorControlStyles.setIconButtonActive(transformScaleAction,
                    state.transformMode == NativeViewport.GIZMO_MODE_SCALE);
        }
        if (state.transformSpaceOffered) {
            EditorControlStyles.setIconButtonActive(transformSpaceWorldAction,
                    state.transformSpace == NativeViewport.GIZMO_SPACE_WORLD);
            EditorControlStyles.setIconButtonActive(transformSpaceLocalAction,
                    state.transformSpace == NativeViewport.GIZMO_SPACE_LOCAL);
        }
        setVisibility(state.displaySuppressed ? GONE : VISIBLE);
    }

    /** The concrete surfaces this host contributes to viewport-occlusion accounting. */
    View[] occludingSurfaces() {
        return new View[]{toolRailScroll, transformModeGroup, transformSpaceGroup,
                precisionGroup};
    }

    @Override
    public void onToolSelected(int key) {
        callbacks.onToolSelected(key);
    }

    private ImageView selectorButton(Context context, int id, int icon, int label,
                                     OnClickListener listener) {
        final ImageView action = EditorControlStyles.iconButton(context, id, icon,
                context.getString(label));
        action.setOnClickListener(listener);
        return action;
    }

    private void ensureEntries(boolean sculpting) {
        if (showingSculptEntries != null && showingSculptEntries == sculpting
                && toolRail.getChildCount() > 0) {
            return;
        }
        showingSculptEntries = sculpting;
        final Context context = getContext();
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
        toolRail.setEntries(new ToolRailView.Entry[]{
                new ToolRailView.Entry(R.id.tool_rail_shape, R.drawable.ic_tool_shape,
                        context.getString(R.string.tool_shape),
                        EditorUiState.CONSTRUCTION_TOOL_SHAPE),
                new ToolRailView.Entry(R.id.tool_rail_place, R.drawable.ic_tool_place,
                        context.getString(R.string.tool_transform),
                        EditorUiState.CONSTRUCTION_TOOL_TRANSFORM),
        });
    }

    private static LinearLayout.LayoutParams selectorFollowerParams(Context context) {
        final LinearLayout.LayoutParams params =
                EditorControlStyles.iconButtonParams(context, 0);
        params.topMargin = EditorControlStyles.dimen(context, R.dimen.toolbar_gap);
        return params;
    }

    private void applySelectorOrientation(boolean compact) {
        applyGroupOrientation(transformModeGroup, compact, transformRotateAction,
                transformScaleAction);
        applyGroupOrientation(transformSpaceGroup, compact, transformSpaceLocalAction);
        applyGroupOrientation(transformSelectorRow, compact, transformSpaceGroup);
    }

    private void applyGroupOrientation(LinearLayout group, boolean compact, View... followers) {
        final int orientation = compact ? LinearLayout.HORIZONTAL : LinearLayout.VERTICAL;
        if (group.getOrientation() == orientation) {
            return;
        }
        group.setOrientation(orientation);
        if (group == transformSelectorRow) {
            group.setGravity(compact ? Gravity.BOTTOM : Gravity.END);
        } else {
            group.setGravity(compact ? Gravity.CENTER_VERTICAL : Gravity.CENTER_HORIZONTAL);
        }
        final int gap = EditorControlStyles.dimen(getContext(), R.dimen.toolbar_gap);
        for (View follower : followers) {
            final ViewGroup.LayoutParams params = follower.getLayoutParams();
            if (params instanceof LinearLayout.LayoutParams) {
                final LinearLayout.LayoutParams typed = (LinearLayout.LayoutParams) params;
                typed.topMargin = compact ? 0 : gap;
                typed.leftMargin = compact ? gap : 0;
                follower.setLayoutParams(typed);
            }
        }
    }
}
