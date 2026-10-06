package com.forgeshape.app;

import android.content.Context;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * The creation surface: choose a shape, and a new Construction Body is that
 * shape — or choose New Sketch, and a CAD Body begins on the plane you pick.
 *
 * <p><b>It builds no geometry and defaults no dimension.</b> Choosing a tile
 * runs exactly the two calls a user would make by hand — the scene's own
 * {@code sceneAddBody()}, then that primitive's own
 * {@code applyConstruction*()} with the parameters <b>read back from the new
 * body's own native state</b> — and then reads the verdict. There is no
 * Java-side generator, no second table of default sizes and no second
 * validation: every body every route creates is built by the same domain code,
 * and a shape chosen here can be refused exactly as a typed one can.
 *
 * <p><b>Two creation categories, and nothing else.</b> Six primitive tiles, one
 * per primitive the product actually builds, and one New Sketch tile as a
 * second group. New Sketch enters the spatial support chooser directly
 * (`UI-OWNER-46`) — a plane or a planar CAD face is picked in the viewport —
 * and the by-name plane list stays reachable from a secondary control as the
 * accessibility fallback; a body exists only once that sketch has been
 * extruded. There is no <i>Add from file</i> here (Import GLB… is the project
 * surface's), no template and no disabled placeholder.
 *
 * <p>It offers no subdivisions, segments or any other topology control. Where a
 * primitive has exact parameters they are its own, and they are edited in the
 * precision surface once it exists.
 *
 * <p>The growth out of the {@code +} is {@link AnchoredSurfaceView}'s, shared
 * with the scene list, the precision surface and the Display popover.
 */
final class AddPrimitivePaletteView extends AnchoredSurfaceView {

    /** Told what the user chose; the caller owns what that means. */
    interface OnPrimitiveChosen {
        void onAddPrimitiveChosen(int primitiveKind);

        /** A sketch on the given {@code WORKPLANE_*}. */
        void onNewSketchChosen(int workplane);

        /**
         * Enter spatial "Choose Sketch Support": pick a world plane OR a planar
         * CAD face directly in the viewport (`CAD-A3`). The primary path; the
         * by-name plane list stays as a fallback.
         */
        void onNewSketchSpatial();

        /**
         * A Freeform body (`MODELING-FOUNDATIONS-R1` B) of the given
         * {@code NativeViewport.FREEFORM_FORM_*}: a control cage the smooth
         * surface is subdivided from.
         */
        void onAddFreeformChosen(int form);
    }

    /** Two rather than three: each tile carries a 26 dp silhouette above its
     *  name, and at three columns "Cylinder" no longer fits under its own icon
     *  on the narrowest window the product supports. */
    private static final int COLUMNS = 2;

    /**
     * The six primitives, in the order the shape editor's chooser already uses,
     * so one closed set has one order in one product.
     *
     * <p>Indexed by the {@code NativeViewport.PRIMITIVE_*} constant, which is
     * what makes the routing a lookup rather than a translation — there is no
     * position in this class at which a tile's index and the domain's kind can
     * disagree.
     */
    private static final int[] TILE_IDS = {
            R.id.add_primitive_box, R.id.add_primitive_cylinder,
            R.id.add_primitive_sphere, R.id.add_primitive_cone,
            R.id.add_primitive_capsule, R.id.add_primitive_plane
    };
    private static final int[] TILE_LABELS = {
            R.string.primitive_box, R.string.primitive_cylinder, R.string.primitive_sphere,
            R.string.primitive_cone, R.string.primitive_capsule, R.string.primitive_plane
    };
    private static final int[] TILE_ICONS = {
            R.drawable.ic_primitive_box, R.drawable.ic_primitive_cylinder,
            R.drawable.ic_primitive_sphere, R.drawable.ic_primitive_cone,
            R.drawable.ic_primitive_capsule, R.drawable.ic_primitive_plane
    };

    /** The three Freeform creation forms, indexed by {@code NativeViewport.FREEFORM_FORM_*}. */
    private static final int[] FREEFORM_IDS = {
            R.id.add_freeform_box, R.id.add_freeform_plane, R.id.add_freeform_cylinder
    };
    private static final int[] FREEFORM_LABELS = {
            R.string.add_freeform_box, R.string.add_freeform_plane, R.string.add_freeform_cylinder
    };
    private static final int[] FREEFORM_ICONS = {
            R.drawable.ic_freeform, R.drawable.ic_primitive_plane, R.drawable.ic_primitive_cylinder
    };

    /** The three planes, indexed by {@code NativeViewport.WORKPLANE_*}. */
    private static final int[] PLANE_IDS = {
            R.id.sketch_plane_xy, R.id.sketch_plane_xz, R.id.sketch_plane_yz
    };
    private static final int[] PLANE_LABELS = {
            R.string.workplane_xy, R.string.workplane_xz, R.string.workplane_yz
    };
    private static final int[] PLANE_DESCRIPTIONS = {
            R.string.workplane_xy_description, R.string.workplane_xz_description,
            R.string.workplane_yz_description
    };

    private final LinearLayout shapesSection;
    private final LinearLayout planeSection;

    AddPrimitivePaletteView(Context context, final OnPrimitiveChosen listener) {
        super(context);
        setId(R.id.add_primitive_palette);
        setContentDescription(context.getString(R.string.add_primitive));
        // TIER 2 — an expanded context surface carrying a body of content to be
        // read and chosen from, exactly like the Objects panel.
        EditorControlStyles.applyContextSurface(this);
        final int pad = EditorControlStyles.dimen(context, R.dimen.inspector_padding);
        setPadding(pad, pad, pad, pad);
        final int gap = EditorControlStyles.dimen(context, R.dimen.row_gap_small);

        // ----- the shapes, and New Sketch -----
        shapesSection = new LinearLayout(context);
        shapesSection.setOrientation(VERTICAL);
        shapesSection.addView(EditorControlStyles.sectionLabel(context,
                        context.getString(R.string.add_primitive)),
                EditorControlStyles.rowParams(0));
        LinearLayout row = null;
        for (int kind = 0; kind < TILE_IDS.length; kind++) {
            if (kind % COLUMNS == 0) {
                row = new LinearLayout(context);
                row.setOrientation(HORIZONTAL);
                shapesSection.addView(row, EditorControlStyles.rowParams(gap));
            }
            final int chosen = kind;
            final View tile = buildTile(context, TILE_IDS[kind], TILE_ICONS[kind],
                    context.getString(TILE_LABELS[kind]));
            tile.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    listener.onAddPrimitiveChosen(chosen);
                }
            });
            row.addView(tile, EditorControlStyles.evenShare(kind % COLUMNS == 0 ? 0 : gap));
        }
        // Freeform: three cages, a peer category of the shapes. A Freeform body
        // has no primitive parameters -- its truth is the cage -- so these are
        // never the shape editor's six.
        shapesSection.addView(EditorControlStyles.sectionLabel(context,
                        context.getString(R.string.new_project_freeform_title)),
                EditorControlStyles.rowParams(EditorControlStyles.dimen(context, R.dimen.section_gap)));
        for (int form = 0; form < FREEFORM_IDS.length; form++) {
            if (form % COLUMNS == 0) {
                row = new LinearLayout(context);
                row.setOrientation(HORIZONTAL);
                shapesSection.addView(row, EditorControlStyles.rowParams(gap));
            }
            final int chosen = form;
            final View tile = buildTile(context, FREEFORM_IDS[form], FREEFORM_ICONS[form],
                    context.getString(FREEFORM_LABELS[form]));
            tile.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    listener.onAddFreeformChosen(chosen);
                }
            });
            row.addView(tile, EditorControlStyles.evenShare(form % COLUMNS == 0 ? 0 : gap));
        }
        if (FREEFORM_IDS.length % COLUMNS != 0) {
            row.addView(new View(context), EditorControlStyles.evenShare(gap));
        }
        // The second category, on its own row and at the same tile height, so
        // it reads as a peer of the shapes and not as a footer.
        final LinearLayout sketchRow = new LinearLayout(context);
        sketchRow.setOrientation(HORIZONTAL);
        // New Sketch lands DIRECTLY in the spatial support chooser
        // (`UI-OWNER-46`): the viewport's planes and faces are the primary way
        // to choose a support, and no intermediary row stands between the tile
        // and them. The by-name plane list is reached from the secondary
        // control under it, and stays as the accessibility fallback.
        final View sketchTile = buildTile(context, R.id.add_sketch, R.drawable.ic_sketch_new,
                context.getString(R.string.new_sketch));
        sketchTile.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                listener.onNewSketchSpatial();
            }
        });
        sketchRow.addView(sketchTile, EditorControlStyles.evenShare(0));
        sketchRow.addView(new View(context), EditorControlStyles.evenShare(gap));
        shapesSection.addView(sketchRow, EditorControlStyles.rowParams(
                EditorControlStyles.dimen(context, R.dimen.section_gap)));
        final TextView byName = EditorControlStyles.secondaryActionChip(context,
                R.id.sketch_plane_by_name, context.getString(R.string.sketch_plane_by_name));
        byName.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                showPlanes(true);
            }
        });
        final LinearLayout.LayoutParams byNameParams = EditorControlStyles.rowParams(gap);
        byNameParams.width = ViewGroup.LayoutParams.MATCH_PARENT;
        shapesSection.addView(byName, byNameParams);
        addView(shapesSection, EditorControlStyles.rowParams(0));

        // ----- the plane chooser -----
        planeSection = new LinearLayout(context);
        planeSection.setId(R.id.sketch_plane_chooser);
        planeSection.setOrientation(VERTICAL);
        planeSection.setVisibility(GONE);
        planeSection.addView(EditorControlStyles.sectionLabel(context,
                        context.getString(R.string.sketch_plane_prompt)),
                EditorControlStyles.rowParams(0));
        // The viewport-first path is offered here too, so a user who opened the
        // by-name list can still go to the viewport from it; the tile above is
        // the primary route.
        final TextView spatial = EditorControlStyles.listRow(context, R.id.sketch_support_spatial,
                context.getString(R.string.sketch_support_spatial));
        spatial.setContentDescription(context.getString(R.string.sketch_support_spatial));
        spatial.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                listener.onNewSketchSpatial();
            }
        });
        final LinearLayout.LayoutParams spatialParams = EditorControlStyles.rowParams(gap);
        spatialParams.width = ViewGroup.LayoutParams.MATCH_PARENT;
        planeSection.addView(spatial, spatialParams);
        for (int plane = 0; plane < PLANE_IDS.length; plane++) {
            final int chosen = plane;
            final TextView option = EditorControlStyles.listRow(context, PLANE_IDS[plane],
                    context.getString(PLANE_DESCRIPTIONS[plane]));
            option.setContentDescription(context.getString(PLANE_LABELS[plane]));
            option.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    listener.onNewSketchChosen(chosen);
                }
            });
            final LinearLayout.LayoutParams params = EditorControlStyles.rowParams(gap);
            params.width = ViewGroup.LayoutParams.MATCH_PARENT;
            planeSection.addView(option, params);
        }
        final TextView back = EditorControlStyles.secondaryActionChip(context,
                R.id.sketch_plane_back, context.getString(R.string.sketch_plane_back));
        back.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                showPlanes(false);
            }
        });
        final LinearLayout.LayoutParams backParams = EditorControlStyles.rowParams(
                EditorControlStyles.dimen(context, R.dimen.section_gap));
        backParams.width = ViewGroup.LayoutParams.MATCH_PARENT;
        planeSection.addView(back, backParams);
        addView(planeSection, EditorControlStyles.rowParams(0));
    }

    /**
     * Shows the plane chooser, or the shapes.
     *
     * <p>The palette always opens on the shapes: a plane is a question that
     * follows New Sketch, and a palette that reopened part way through a
     * previous answer would be asking a question nobody asked.
     */
    void showPlanes(boolean planes) {
        shapesSection.setVisibility(planes ? GONE : VISIBLE);
        planeSection.setVisibility(planes ? VISIBLE : GONE);
    }

    /** Whether the plane chooser is on screen, for verification. */
    boolean showingPlanes() {
        return planeSection.getVisibility() == VISIBLE;
    }

    /**
     * One tile: a silhouette with its name under it.
     *
     * <p>The silhouette is the point. Six words are a list of primitives; six
     * shapes are a choice between them, and on a phone the outline is what is
     * read first. The name stays because an outline alone cannot tell a capsule
     * from a cylinder at 26 dp.
     */
    private View buildTile(Context context, int id, int iconRes, String label) {
        final LinearLayout tile = new LinearLayout(context);
        tile.setId(id);
        tile.setOrientation(VERTICAL);
        tile.setGravity(Gravity.CENTER);
        tile.setBackgroundResource(R.drawable.bg_control);
        tile.setClickable(true);
        tile.setFocusable(true);
        tile.setMinimumHeight(
                EditorControlStyles.dimen(context, R.dimen.add_primitive_tile_height));

        tile.addView(EditorControlStyles.icon(context, iconRes, R.dimen.add_primitive_icon_size));

        final TextView caption = new TextView(context);
        caption.setText(label);
        caption.setGravity(Gravity.CENTER);
        caption.setSingleLine(true);
        caption.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                EditorControlStyles.dimen(context, R.dimen.text_body));
        // Icon and label read the same state list and both duplicate the tile's
        // state, so a tile's glyph and its caption cannot disagree about whether
        // it is pressed.
        caption.setTextColor(EditorControlStyles.contentTint(context));
        caption.setDuplicateParentStateEnabled(true);
        final LinearLayout.LayoutParams labelParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        labelParams.topMargin = EditorControlStyles.dimen(context, R.dimen.row_gap_small);
        tile.addView(caption, labelParams);

        tile.setContentDescription(label);
        return tile;
    }

    /**
     * The palette's layout in the overlay, at its own fixed width.
     *
     * <p>The width is fixed rather than measured because the anchor has to
     * place the surface before it has ever been laid out — see
     * {@code EditorWorkspaceView.anchorOverlayTo}.
     */
    static ViewGroup.LayoutParams anchoredParams(Context context) {
        final FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                EditorControlStyles.dimen(context, R.dimen.add_primitive_palette_width),
                ViewGroup.LayoutParams.WRAP_CONTENT);
        params.gravity = Gravity.BOTTOM | Gravity.START;
        return params;
    }
}
