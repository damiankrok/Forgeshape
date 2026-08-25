package com.forgeshape.app;

import android.content.Context;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.MotionEvent;
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
 */
final class AddPrimitivePaletteView extends LinearLayout {

    /** Told which primitive the user chose; the caller owns what that means. */
    interface OnPrimitiveChosen {
        void onAddPrimitiveChosen(int primitiveKind);
    }

    /** Matches the growth of every other context surface, so the two panels the
     *  Objects capsule opens feel like one system. */
    private static final long OPEN_DURATION_MS = 140L;
    private static final long CLOSE_DURATION_MS = 100L;

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

    /** Set by the anchor, so the surface grows out of the control that opened
     *  it rather than out of a window edge. See {@link #setGrowsUpward}. */
    private boolean growsUpward = true;

    /** The state the user asked for; see {@link #isOpen()}. */
    private boolean open;

    AddPrimitivePaletteView(Context context, final OnPrimitiveChosen listener) {
        super(context);
        setId(R.id.add_primitive_palette);
        setOrientation(VERTICAL);
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

        setVisibility(GONE);
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
     * Whether the palette is open.
     *
     * <p>The <b>target</b> state, not this frame's visibility. A closing surface
     * is still VISIBLE for the length of its fade, and the control that draws
     * itself active while the palette is up would otherwise stay lit for good
     * after a shape was chosen.
     */
    boolean isOpen() {
        return open;
    }

    /**
     * Which way the surface grows, decided by where the control that opened it
     * sits in the window.
     *
     * <p>The Objects capsule is low on a phone and the docked Objects column is
     * high on a tablet, and in both cases the palette has to appear to come
     * <i>out of</i> the {@code +} rather than to arrive from an unrelated edge.
     */
    void setGrowsUpward(boolean upward) {
        growsUpward = upward;
    }

    /** Opens or closes the palette, growing from the control that opened it. */
    void setOpen(boolean open) {
        if (open == this.open) {
            return;
        }
        this.open = open;
        // Always interruptible: a second tap while the open animation is still
        // running must close it, not queue behind it.
        ChromeMotion.begin(this);

        // The leading edge is where the plus is in both hosts, so the surface
        // unfolds from that corner rather than from its own centre.
        setPivotX(0.0f);
        setPivotY(growsUpward ? getHeight() : 0.0f);

        if (!ChromeMotion.animationsEnabled(getContext())) {
            settle(open);
            return;
        }
        if (open) {
            setAlpha(0.0f);
            setScaleX(0.96f);
            setScaleY(0.96f);
            setVisibility(VISIBLE);
            animate().alpha(1.0f).scaleX(1.0f).scaleY(1.0f)
                    .setDuration(OPEN_DURATION_MS).start();
        } else {
            animate().alpha(0.0f).scaleX(0.96f).scaleY(0.96f)
                    .setDuration(CLOSE_DURATION_MS)
                    .withEndAction(new Runnable() {
                        @Override
                        public void run() {
                            settle(false);
                        }
                    }).start();
        }
    }

    /**
     * Closes with no animation, for the case where the chrome around the
     * palette is disappearing in the same frame.
     */
    void closeImmediately() {
        open = false;
        ChromeMotion.begin(this);
        settle(false);
    }

    /** Lands on a resting state, so the next open starts from a known transform
     *  rather than from wherever a cancelled animation stopped. */
    private void settle(boolean open) {
        setVisibility(open ? VISIBLE : GONE);
        setAlpha(1.0f);
        setScaleX(1.0f);
        setScaleY(1.0f);
    }

    /**
     * Swallows every touch the palette's own tiles did not take.
     *
     * <p>The same rule every chrome surface follows: a missed tap between two
     * tiles must not reach the {@code SurfaceView} beneath and orbit the model.
     */
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        super.onTouchEvent(event);
        return true;
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
