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
 * shape.
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
 * <p><b>Exactly six tiles, and nothing else.</b> One per primitive the product
 * actually builds. There is deliberately no <i>Add from file</i>, no template,
 * no import and no disabled placeholder: a creation action ForgeShape cannot
 * honour would be a promise the shell cannot keep. The seam for a later one is
 * structural rather than drawn — this view owns its own layout and its own
 * routing, so a second creation <i>category</i> becomes a second group inside
 * it without Objects, the scene or the capsule learning anything new.
 *
 * <p>It offers no subdivisions, segments or any other topology control. Where a
 * primitive has exact parameters they are its own, and they are edited in the
 * precision surface once it exists.
 *
 * <p>The growth out of the {@code +} is {@link AnchoredSurfaceView}'s, shared
 * with the scene list, the precision surface and the Display popover. It used to
 * be a private copy here, which is how this surface came to open with a
 * different duration and a different pivot from the panel beside it.
 */
final class AddPrimitivePaletteView extends AnchoredSurfaceView {

    /** Told which primitive the user chose; the caller owns what that means. */
    interface OnPrimitiveChosen {
        void onAddPrimitiveChosen(int primitiveKind);
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

    AddPrimitivePaletteView(Context context, final OnPrimitiveChosen listener) {
        super(context);
        setId(R.id.add_primitive_palette);
        setContentDescription(context.getString(R.string.add_primitive));
        // TIER 2 — an expanded context surface carrying a body of content to be
        // read and chosen from, exactly like the Objects panel.
        EditorControlStyles.applyContextSurface(this);
        final int pad = EditorControlStyles.dimen(context, R.dimen.inspector_padding);
        setPadding(pad, pad, pad, pad);

        addView(EditorControlStyles.sectionLabel(context,
                        context.getString(R.string.add_primitive)),
                EditorControlStyles.rowParams(0));

        final int gap = EditorControlStyles.dimen(context, R.dimen.row_gap_small);
        LinearLayout row = null;
        for (int kind = 0; kind < TILE_IDS.length; kind++) {
            if (kind % COLUMNS == 0) {
                row = new LinearLayout(context);
                row.setOrientation(HORIZONTAL);
                addView(row, EditorControlStyles.rowParams(gap));
            }
            final int chosen = kind;
            final View tile = buildTile(context, kind);
            tile.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    listener.onAddPrimitiveChosen(chosen);
                }
            });
            row.addView(tile, EditorControlStyles.evenShare(kind % COLUMNS == 0 ? 0 : gap));
        }
    }

    /**
     * One shape tile: a silhouette with its name under it.
     *
     * <p>The silhouette is the point. Six words are a list of primitives; six
     * shapes are a choice between them, and on a phone the outline is what is
     * read first. The name stays because an outline alone cannot tell a capsule
     * from a cylinder at 26 dp.
     */
    private View buildTile(Context context, int kind) {
        final LinearLayout tile = new LinearLayout(context);
        tile.setId(TILE_IDS[kind]);
        tile.setOrientation(VERTICAL);
        tile.setGravity(Gravity.CENTER);
        tile.setBackgroundResource(R.drawable.bg_control);
        tile.setClickable(true);
        tile.setFocusable(true);
        tile.setMinimumHeight(
                EditorControlStyles.dimen(context, R.dimen.add_primitive_tile_height));

        tile.addView(EditorControlStyles.icon(context, TILE_ICONS[kind],
                R.dimen.add_primitive_icon_size));

        final TextView label = new TextView(context);
        label.setText(context.getString(TILE_LABELS[kind]));
        label.setGravity(Gravity.CENTER);
        label.setSingleLine(true);
        label.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                EditorControlStyles.dimen(context, R.dimen.text_body));
        // Icon and label read the same state list and both duplicate the tile's
        // state, so a tile's glyph and its caption cannot disagree about whether
        // it is pressed.
        label.setTextColor(EditorControlStyles.contentTint(context));
        label.setDuplicateParentStateEnabled(true);
        final LinearLayout.LayoutParams labelParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        labelParams.topMargin = EditorControlStyles.dimen(context, R.dimen.row_gap_small);
        tile.addView(label, labelParams);

        tile.setContentDescription(context.getString(TILE_LABELS[kind]));
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
