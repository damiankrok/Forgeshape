package com.forgeshape.app;

import android.content.Context;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.HorizontalScrollView;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * The tablet workbench's <b>ribbon</b> (`WORKBENCH-RIBBON-R2`): the desktop's
 * captioned command row, under the Global Toolbar, in tabs and named groups
 * — the SolidCraft arrangement the OWNER asked both shells to share.
 *
 * <p><b>Model</b> tab: <i>Sketch</i> (New sketch), <i>Primitives</i> (the six
 * shapes), <i>Part</i> (Make CAD part, drawn only where the primitive can
 * become one), <i>Arrange</i> (Move, Rotate, Scale, World / Local where a
 * space applies, Delete never for the last body) and the contextual
 * <i>Face</i> group (Sketch here, Round edges, Bevel edges, Hole, Pattern,
 * Mirror) drawn exactly when the touched face offers those acts — on a
 * workbench the ribbon is their home and the docked face column is
 * withdrawn, so each act keeps ONE home per layout. <b>View</b> tab:
 * <i>Show</i> (Grid, Outline, Edges). The desktop's Sketch tab is the Tool
 * Rail's on a tablet: a sketch owns the viewport and the ribbon stands down.
 *
 * <p><b>It decides nothing.</b> Every command calls the ONE handler the
 * workspace already had for that act, and what is drawn is
 * {@link WorkbenchRibbonPresentation}'s answer over native facts the
 * workspace reads on every refresh. A command is a rail entry — a 22 dp glyph
 * over a 10 sp caption on a 56 × 64 dp target — so the ribbon and the rail
 * are one grammar; the held tab and the held command wear the product's one
 * selected treatment.
 */
final class WorkbenchRibbonView extends LinearLayout {

    /** The acts, each the workspace's existing handler. */
    interface Host {
        void onRibbonNewSketch();

        void onRibbonAddPrimitive(int primitiveKind);

        void onRibbonMakeCadPart();

        void onRibbonTransformMode(int mode);

        void onRibbonTransformSpace(int space);

        void onRibbonDeleteActive();

        /** One of {@code WorkbenchRibbonPresentation.ACT_*}. */
        void onRibbonFaceAct(int act);

        void onRibbonGridVisible(boolean visible);

        void onRibbonOutlineVisible(boolean visible);

        void onRibbonEdgesVisible(boolean visible);
    }

    private static final int[] PRIMITIVE_IDS = {
            R.id.ribbon_add_box, R.id.ribbon_add_cylinder, R.id.ribbon_add_sphere,
            R.id.ribbon_add_cone, R.id.ribbon_add_capsule, R.id.ribbon_add_plane
    };
    private static final int[] PRIMITIVE_LABELS = {
            R.string.primitive_box, R.string.primitive_cylinder, R.string.primitive_sphere,
            R.string.primitive_cone, R.string.primitive_capsule, R.string.primitive_plane
    };
    private static final int[] PRIMITIVE_ICONS = {
            R.drawable.ic_primitive_box, R.drawable.ic_primitive_cylinder,
            R.drawable.ic_primitive_sphere, R.drawable.ic_primitive_cone,
            R.drawable.ic_primitive_capsule, R.drawable.ic_primitive_plane
    };

    private final Host host;
    private final TextView modelTab;
    private final TextView viewTab;
    private final LinearLayout modelRow;
    private final LinearLayout viewRow;
    private final View partGroup;
    private final View faceGroup;
    private final View move;
    private final View rotate;
    private final View scale;
    private final View world;
    private final View local;
    private final View delete;
    private final View sketchHere;
    private final View fillet;
    private final View chamfer;
    private final View hole;
    private final View pattern;
    private final View mirror;
    private final View grid;
    private final View outline;
    private final View edges;
    private int tab = WorkbenchRibbonPresentation.TAB_MODEL;
    private WorkbenchRibbonPresentation.State state;

    WorkbenchRibbonView(Context context, Host host) {
        super(context);
        this.host = host;
        setId(R.id.workbench_ribbon);
        setOrientation(VERTICAL);
        // Part of the window frame, like the desktop's: a docked chrome
        // surface with a hairline under it, never a capsule standing on the
        // model. It consumes its own touches so a reach for a command cannot
        // orbit the camera under it.
        EditorControlStyles.applyChromeSurface(this);
        setClickable(true);
        final int padH = EditorControlStyles.dimen(context, R.dimen.toolbar_padding_horizontal);
        final int gap = EditorControlStyles.dimen(context, R.dimen.row_gap_small);
        setPadding(padH, 0, padH, gap);

        // --- Tabs ------------------------------------------------------------
        final LinearLayout tabs = new LinearLayout(context);
        tabs.setOrientation(HORIZONTAL);
        tabs.setGravity(Gravity.CENTER_VERTICAL);
        modelTab = EditorControlStyles.chip(context, R.id.ribbon_tab_model,
                context.getString(R.string.ribbon_tab_model));
        modelTab.setOnClickListener(v -> selectTab(WorkbenchRibbonPresentation.TAB_MODEL));
        tabs.addView(modelTab, EditorControlStyles.wrap(0));
        viewTab = EditorControlStyles.chip(context, R.id.ribbon_tab_view,
                context.getString(R.string.ribbon_tab_view));
        viewTab.setOnClickListener(v -> selectTab(WorkbenchRibbonPresentation.TAB_VIEW));
        tabs.addView(viewTab, EditorControlStyles.wrap(gap));
        addView(tabs, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));

        // --- The command rows, one per tab, scrolling sideways on a window
        // too narrow for every group ---------------------------------------
        final HorizontalScrollView scroller = new HorizontalScrollView(context);
        scroller.setHorizontalScrollBarEnabled(false);
        final LinearLayout rows = new LinearLayout(context);
        rows.setOrientation(VERTICAL);
        scroller.addView(rows, new ViewGroup.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));
        addView(scroller, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));

        modelRow = commandRow(context, R.id.ribbon_model_row);
        rows.addView(modelRow, EditorControlStyles.wrap(0));
        viewRow = commandRow(context, R.id.ribbon_view_row);
        viewRow.setVisibility(GONE);
        rows.addView(viewRow, EditorControlStyles.wrap(0));

        // Model: Sketch.
        LinearLayout group = beginGroup(context, modelRow, R.id.ribbon_group_sketch,
                R.string.ribbon_group_sketch, true);
        addCommand(context, group, R.id.ribbon_new_sketch, R.drawable.ic_sketch_new,
                R.string.ribbon_new_sketch, R.string.new_sketch, v -> host.onRibbonNewSketch());
        // Model: Primitives.
        group = beginGroup(context, modelRow, R.id.ribbon_group_primitives,
                R.string.ribbon_group_primitives, false);
        for (int i = 0; i < PRIMITIVE_IDS.length; i++) {
            final int kind = i;
            addCommand(context, group, PRIMITIVE_IDS[i], PRIMITIVE_ICONS[i], PRIMITIVE_LABELS[i],
                    PRIMITIVE_LABELS[i], v -> host.onRibbonAddPrimitive(kind));
        }
        // Model: Part, drawn only where the primitive can become one.
        partGroup = beginGroup(context, modelRow, R.id.ribbon_group_part,
                R.string.ribbon_group_part, false);
        addCommand(context, (LinearLayout) partGroup, R.id.ribbon_cad_part, R.drawable.ic_cad_part,
                R.string.ribbon_cad_part, R.string.make_cad_part, v -> host.onRibbonMakeCadPart());
        // Model: Arrange.
        group = beginGroup(context, modelRow, R.id.ribbon_group_arrange,
                R.string.ribbon_group_arrange, false);
        move = addCommand(context, group, R.id.ribbon_move, R.drawable.ic_gizmo_move,
                R.string.rail_caption_move, R.string.transform_mode_move,
                v -> host.onRibbonTransformMode(NativeViewport.GIZMO_MODE_MOVE));
        rotate = addCommand(context, group, R.id.ribbon_rotate, R.drawable.ic_gizmo_rotate,
                R.string.rail_caption_rotate, R.string.transform_mode_rotate,
                v -> host.onRibbonTransformMode(NativeViewport.GIZMO_MODE_ROTATE));
        scale = addCommand(context, group, R.id.ribbon_scale, R.drawable.ic_gizmo_scale,
                R.string.rail_caption_scale, R.string.transform_mode_scale,
                v -> host.onRibbonTransformMode(NativeViewport.GIZMO_MODE_SCALE));
        world = addCommand(context, group, R.id.ribbon_space_world, R.drawable.ic_space_world,
                R.string.rail_caption_world, R.string.transform_space_world,
                v -> host.onRibbonTransformSpace(NativeViewport.GIZMO_SPACE_WORLD));
        local = addCommand(context, group, R.id.ribbon_space_local, R.drawable.ic_space_local,
                R.string.rail_caption_local, R.string.transform_space_local,
                v -> host.onRibbonTransformSpace(NativeViewport.GIZMO_SPACE_LOCAL));
        delete = addCommand(context, group, R.id.ribbon_delete, R.drawable.ic_delete,
                R.string.ribbon_delete, R.string.ribbon_delete_description,
                v -> host.onRibbonDeleteActive());
        // Model: the contextual Face group.
        faceGroup = beginGroup(context, modelRow, R.id.ribbon_group_face,
                R.string.ribbon_group_face, false);
        final LinearLayout face = (LinearLayout) faceGroup;
        sketchHere = addCommand(context, face, R.id.ribbon_sketch_here, R.drawable.ic_sketch_new,
                R.string.sketch_on_face_caption, R.string.sketch_on_face_description,
                v -> host.onRibbonFaceAct(WorkbenchRibbonPresentation.ACT_SKETCH_HERE));
        fillet = addCommand(context, face, R.id.ribbon_fillet, R.drawable.ic_edge_fillet,
                R.string.ribbon_round, R.string.ribbon_round_description,
                v -> host.onRibbonFaceAct(WorkbenchRibbonPresentation.ACT_FILLET));
        chamfer = addCommand(context, face, R.id.ribbon_chamfer, R.drawable.ic_edge_chamfer,
                R.string.ribbon_bevel, R.string.ribbon_bevel_description,
                v -> host.onRibbonFaceAct(WorkbenchRibbonPresentation.ACT_CHAMFER));
        hole = addCommand(context, face, R.id.ribbon_hole, R.drawable.ic_hole,
                R.string.hole_action, R.string.hole_action_description,
                v -> host.onRibbonFaceAct(WorkbenchRibbonPresentation.ACT_HOLE));
        pattern = addCommand(context, face, R.id.ribbon_pattern, R.drawable.ic_pattern_linear,
                R.string.pattern_action, R.string.pattern_action_description,
                v -> host.onRibbonFaceAct(WorkbenchRibbonPresentation.ACT_PATTERN));
        mirror = addCommand(context, face, R.id.ribbon_mirror, R.drawable.ic_pattern_mirror,
                R.string.mirror_action, R.string.mirror_action_description,
                v -> host.onRibbonFaceAct(WorkbenchRibbonPresentation.ACT_MIRROR));

        // View: Show.
        group = beginGroup(context, viewRow, R.id.ribbon_group_show, R.string.ribbon_group_show,
                true);
        grid = addCommand(context, group, R.id.ribbon_grid, R.drawable.ic_view_grid,
                R.string.ribbon_grid, R.string.ribbon_grid,
                v -> host.onRibbonGridVisible(!v.isActivated()));
        outline = addCommand(context, group, R.id.ribbon_outline, R.drawable.ic_view_outline,
                R.string.ribbon_outline, R.string.ribbon_outline_description,
                v -> host.onRibbonOutlineVisible(!v.isActivated()));
        edges = addCommand(context, group, R.id.ribbon_edges, R.drawable.ic_view_edges,
                R.string.ribbon_edges, R.string.ribbon_edges_description,
                v -> host.onRibbonEdgesVisible(!v.isActivated()));

        selectTab(WorkbenchRibbonPresentation.TAB_MODEL);
    }

    /** Redraws every group and command from one snapshot of native facts. */
    void show(WorkbenchRibbonPresentation.State state) {
        this.state = state;
        partGroup.setVisibility(
                WorkbenchRibbonPresentation.partGroupShown(state) ? VISIBLE : GONE);
        setActive(move, WorkbenchRibbonPresentation.transformModeActive(state,
                NativeViewport.GIZMO_MODE_MOVE));
        setActive(rotate, WorkbenchRibbonPresentation.transformModeActive(state,
                NativeViewport.GIZMO_MODE_ROTATE));
        setActive(scale, WorkbenchRibbonPresentation.transformModeActive(state,
                NativeViewport.GIZMO_MODE_SCALE));
        final boolean space = WorkbenchRibbonPresentation.spaceShown(state);
        world.setVisibility(space ? VISIBLE : GONE);
        local.setVisibility(space ? VISIBLE : GONE);
        setActive(world, WorkbenchRibbonPresentation.transformSpaceActive(state,
                NativeViewport.GIZMO_SPACE_WORLD));
        setActive(local, WorkbenchRibbonPresentation.transformSpaceActive(state,
                NativeViewport.GIZMO_SPACE_LOCAL));
        delete.setVisibility(WorkbenchRibbonPresentation.deleteShown(state) ? VISIBLE : GONE);
        faceGroup.setVisibility(
                WorkbenchRibbonPresentation.faceGroupShown(state) ? VISIBLE : GONE);
        showFaceAct(sketchHere, WorkbenchRibbonPresentation.ACT_SKETCH_HERE);
        showFaceAct(fillet, WorkbenchRibbonPresentation.ACT_FILLET);
        showFaceAct(chamfer, WorkbenchRibbonPresentation.ACT_CHAMFER);
        showFaceAct(hole, WorkbenchRibbonPresentation.ACT_HOLE);
        showFaceAct(pattern, WorkbenchRibbonPresentation.ACT_PATTERN);
        showFaceAct(mirror, WorkbenchRibbonPresentation.ACT_MIRROR);
        setActive(grid, state.gridVisible);
        setActive(outline, state.outlineVisible);
        setActive(edges, state.edgesVisible);
    }

    private void showFaceAct(View command, int act) {
        command.setVisibility(
                WorkbenchRibbonPresentation.faceActShown(state, act) ? VISIBLE : GONE);
    }

    /** The held tab, for verification. */
    int tab() {
        return tab;
    }

    private void selectTab(int which) {
        tab = which;
        final boolean model = which == WorkbenchRibbonPresentation.TAB_MODEL;
        EditorControlStyles.setChipActive(modelTab, model);
        EditorControlStyles.setChipActive(viewTab, !model);
        modelRow.setVisibility(model ? VISIBLE : GONE);
        viewRow.setVisibility(model ? GONE : VISIBLE);
    }

    private static void setActive(View command, boolean active) {
        command.setBackgroundResource(
                active ? R.drawable.bg_rail_entry_active : R.drawable.bg_rail_entry);
        command.setActivated(active);
    }

    private static LinearLayout commandRow(Context context, int id) {
        final LinearLayout row = new LinearLayout(context);
        row.setId(id);
        row.setOrientation(HORIZONTAL);
        row.setGravity(Gravity.TOP);
        return row;
    }

    /**
     * One named group: its commands side by side, its name under them, and a
     * hairline before every group but the first — the desktop's own
     * {@code RibbonRow.endGroup} drawn with views.
     */
    private LinearLayout beginGroup(Context context, LinearLayout row, int id, int nameRes,
                                    boolean first) {
        final int gap = EditorControlStyles.dimen(context, R.dimen.row_gap);
        if (!first) {
            final View rule = new View(context);
            rule.setBackgroundColor(EditorControlStyles.themeColor(context,
                    R.attr.fsChromeHairline));
            final LinearLayout.LayoutParams ruleParams = new LinearLayout.LayoutParams(
                    EditorControlStyles.dimen(context, R.dimen.control_border_width),
                    EditorControlStyles.dimen(context, R.dimen.rail_item_height));
            ruleParams.leftMargin = gap;
            ruleParams.rightMargin = gap;
            ruleParams.topMargin = EditorControlStyles.dimen(context, R.dimen.row_gap_small);
            row.addView(rule, ruleParams);
        }
        final LinearLayout group = new LinearLayout(context);
        group.setId(id);
        group.setOrientation(VERTICAL);
        group.setGravity(Gravity.CENTER_HORIZONTAL);
        final LinearLayout commands = new LinearLayout(context);
        commands.setOrientation(HORIZONTAL);
        group.addView(commands, EditorControlStyles.wrap(0));
        final TextView name = new TextView(context);
        name.setText(nameRes);
        name.setGravity(Gravity.CENTER);
        name.setSingleLine(true);
        name.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                EditorControlStyles.dimen(context, R.dimen.text_label));
        name.setTextColor(EditorControlStyles.themeColor(context, R.attr.fsTextSecondary));
        name.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        group.addView(name, EditorControlStyles.rowParams(
                EditorControlStyles.dimen(context, R.dimen.row_gap_small)));
        row.addView(group, EditorControlStyles.wrap(0));
        return group;
    }

    /** A command: a rail entry, so the ribbon and the rail are one grammar. */
    private View addCommand(Context context, LinearLayout group, int id, int iconRes,
                            int captionRes, int descriptionRes, OnClickListener listener) {
        final LinearLayout commands = (LinearLayout) group.getChildAt(0);
        final ToolRailView.RailEntryView entry = new ToolRailView.RailEntryView(context);
        entry.setId(id);
        entry.setOrientation(VERTICAL);
        entry.setGravity(Gravity.CENTER);
        entry.setBackgroundResource(R.drawable.bg_rail_entry);
        final ImageView icon = EditorControlStyles.icon(context, iconRes, R.dimen.rail_icon_size);
        entry.addView(icon, icon.getLayoutParams());
        final TextView caption = new TextView(context);
        caption.setText(captionRes);
        caption.setGravity(Gravity.CENTER);
        caption.setSingleLine(true);
        caption.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                EditorControlStyles.dimen(context, R.dimen.rail_label_size));
        caption.setTextColor(EditorControlStyles.contentTint(context));
        caption.setDuplicateParentStateEnabled(true);
        entry.addView(caption, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        entry.setContentDescription(context.getString(descriptionRes));
        entry.setClickable(true);
        entry.setFocusable(true);
        entry.setOnClickListener(listener);
        final LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                EditorControlStyles.dimen(context, R.dimen.rail_item_width),
                EditorControlStyles.dimen(context, R.dimen.rail_item_height));
        if (commands.getChildCount() > 0) {
            params.leftMargin = EditorControlStyles.dimen(context, R.dimen.rail_item_gap);
        }
        commands.addView(entry, params);
        return entry;
    }
}
