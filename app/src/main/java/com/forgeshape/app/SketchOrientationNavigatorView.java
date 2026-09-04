package com.forgeshape.app;

import android.content.Context;
import android.view.Gravity;
import android.view.ViewGroup;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * The sketch orientation navigator (`SKETCH-UX-R1` C).
 *
 * <p>A compact translucent control in the <b>upper trailing corner</b> of the
 * viewport while a sketch is open, and absent everywhere else. It answers two
 * questions the user has constantly while sketching — <i>which plane am I on,
 * and which way up is it?</i> — and lets them change both without leaving the
 * drawing.
 *
 * <p>Three groups, in one column:
 *
 * <ul>
 *   <li><b>The plane ring</b> — XY, XZ, YZ. The current one is marked. Enabled
 *       only while the sketch is EMPTY and world-supported: once there is
 *       geometry the authored coordinates mean one plane, and switching would
 *       silently reinterpret them. A face-supported sketch never offers it at
 *       all, because its support is a reference to another body's face.</li>
 *   <li><b>The normal</b> — a single control that flips between looking along
 *       the plane's positive and negative normal. It always names the direction
 *       currently being looked along.</li>
 *   <li><b>The rotation arrows</b> — ±90° about the current view normal. They
 *       turn the VIEW; they never touch one authored coordinate and never
 *       mirror anything.</li>
 * </ul>
 *
 * <p><b>Holds no orientation of its own.</b> Every control reads its state from
 * {@code sketchViewState} on each refresh and sends an act back; the session is
 * the one answer, exactly as it is for the tool and the selection. A refused
 * act — switching plane with geometry drawn — is reported by name and changes
 * nothing, including the marking on this control.
 *
 * <p>The by-name plane list in the overflow stays as it was: this is the normal
 * path, not the only one (`CADUXR1-08`, accessibility fallback).
 */
final class SketchOrientationNavigatorView extends LinearLayout {

    /** Told what the user asked for; the workspace owns what it means. */
    interface OnOrientationAction {
        void onSketchPlaneChosen(int workplane);

        void onSketchViewFlipRequested(boolean flipped);

        void onSketchViewRotateRequested(int quarterTurns);
    }

    private final OnOrientationAction actions;
    private final double[] state = new double[NativeViewport.SKETCH_VIEW_SIZE];

    private final TextView[] planeChips = new TextView[3];
    private final TextView normalChip;
    private final TextView rotateLeft;
    private final TextView rotateRight;
    private final TextView viewLabel;

    private static final int[] PLANE_IDS = {
            R.id.sketch_navigator_plane_xy,
            R.id.sketch_navigator_plane_xz,
            R.id.sketch_navigator_plane_yz,
    };
    private static final int[] PLANE_LABELS = {
            R.string.workplane_xy, R.string.workplane_xz, R.string.workplane_yz,
    };

    SketchOrientationNavigatorView(Context context, OnOrientationAction actions) {
        super(context);
        this.actions = actions;
        setId(R.id.sketch_orientation_navigator);
        setOrientation(VERTICAL);
        setGravity(Gravity.CENTER_HORIZONTAL);
        setBackgroundResource(R.drawable.bg_sketch_navigator);
        setElevation(EditorControlStyles.dimen(context, R.dimen.elevation_floating));
        final int pad = EditorControlStyles.dimen(context, R.dimen.sketch_navigator_padding);
        setPadding(pad, pad, pad, pad);
        final int gap = EditorControlStyles.dimen(context, R.dimen.row_gap_small);

        // What is being looked at, in words, above the controls that change it.
        // The control that changes a thing is not the same as the statement of
        // what it currently is, and on a navigator both are wanted at once.
        viewLabel = EditorControlStyles.sectionLabel(context, "");
        viewLabel.setId(R.id.sketch_navigator_view_label);
        viewLabel.setGravity(Gravity.CENTER);
        addView(viewLabel, EditorControlStyles.rowParams(0));

        // The plane ring.
        final LinearLayout planes = new LinearLayout(context);
        planes.setOrientation(HORIZONTAL);
        for (int i = 0; i < 3; i++) {
            final int index = i;
            planeChips[i] = EditorControlStyles.chip(context, PLANE_IDS[i],
                    context.getString(PLANE_LABELS[i]));
            planeChips[i].setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    SketchOrientationNavigatorView.this.actions.onSketchPlaneChosen(index);
                }
            });
            planes.addView(planeChips[i], EditorControlStyles.evenShare(i == 0 ? 0 : gap));
        }
        addView(planes, EditorControlStyles.rowParams(gap));

        // The normal, and the two rotation arrows, on one row: they are the
        // three things that change how the same sketch is presented.
        final LinearLayout view = new LinearLayout(context);
        view.setOrientation(HORIZONTAL);
        rotateLeft = arrow(context, R.id.sketch_navigator_rotate_ccw,
                context.getString(R.string.sketch_rotate_view_ccw_glyph),
                context.getString(R.string.sketch_rotate_view_ccw), -1);
        normalChip = EditorControlStyles.chip(context, R.id.sketch_navigator_flip, "");
        normalChip.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                SketchOrientationNavigatorView.this.actions.onSketchViewFlipRequested(
                        state[NativeViewport.SKETCH_VIEW_FLIPPED] == 0.0);
            }
        });
        rotateRight = arrow(context, R.id.sketch_navigator_rotate_cw,
                context.getString(R.string.sketch_rotate_view_cw_glyph),
                context.getString(R.string.sketch_rotate_view_cw), 1);
        // The two arrows keep their own width and the normal chip takes what is
        // left: rowParams is MATCH_PARENT, which in a horizontal row would give
        // the first child everything and push the other two off the end.
        view.addView(rotateLeft, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        final LinearLayout.LayoutParams normalParams = new LinearLayout.LayoutParams(
                0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
        normalParams.leftMargin = gap;
        normalParams.rightMargin = gap;
        view.addView(normalChip, normalParams);
        view.addView(rotateRight, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        addView(view, EditorControlStyles.rowParams(gap));

        refreshFromNative();
    }

    /**
     * One rotation arrow.
     *
     * <p>The glyph stays the size it reads at and the 48 dp floor is reached
     * with padding, never by growing the drawn mark.
     */
    private TextView arrow(Context context, int id, CharSequence glyph, CharSequence description,
                           final int quarterTurns) {
        final TextView chip = EditorControlStyles.chip(context, id, glyph);
        chip.setContentDescription(description);
        chip.setMinimumWidth(EditorControlStyles.dimen(context, R.dimen.control_height));
        chip.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                actions.onSketchViewRotateRequested(quarterTurns);
            }
        });
        return chip;
    }

    /**
     * Re-reads the session and re-marks every control.
     *
     * <p>The one direction state travels: native says what is true, this shows
     * it. Nothing here remembers a plane, a flip or a roll between refreshes.
     */
    void refreshFromNative() {
        final Context context = getContext();
        NativeViewport.sketchViewState(state);
        final int plane = (int) state[NativeViewport.SKETCH_VIEW_PLANE];
        final boolean flipped = state[NativeViewport.SKETCH_VIEW_FLIPPED] != 0.0;
        final boolean faceSupported = state[NativeViewport.SKETCH_VIEW_FACE_SUPPORTED] != 0.0;
        final boolean switchable = state[NativeViewport.SKETCH_VIEW_PLANE_SWITCHABLE] != 0.0;
        final int quarterTurns = (int) state[NativeViewport.SKETCH_VIEW_QUARTER_TURNS];

        for (int i = 0; i < planeChips.length; i++) {
            EditorControlStyles.setChipActive(planeChips[i], !faceSupported && i == plane);
            // A control that cannot succeed is not offered: with geometry
            // drawn, or on a face support, the plane is fixed and the chips say
            // so rather than being tapped and refused.
            EditorControlStyles.setChipEnabled(planeChips[i], switchable);
            planeChips[i].setVisibility(faceSupported ? GONE : VISIBLE);
        }

        final String normalName = faceSupported
                ? context.getString(R.string.sketch_view_face_normal)
                : context.getString(flipped ? R.string.sketch_view_normal_negative
                                            : R.string.sketch_view_normal_positive,
                                    context.getString(planeNormalAxis(plane)));
        normalChip.setText(normalName);
        normalChip.setContentDescription(
                context.getString(R.string.sketch_view_flip_description, normalName));
        EditorControlStyles.setChipActive(normalChip, flipped);
        // A face sketch has no world normal to flip TO: its support is the
        // producer's face, and looking at the back of it is not a plane choice.
        EditorControlStyles.setChipEnabled(normalChip, !faceSupported);

        viewLabel.setText(faceSupported
                ? context.getString(R.string.sketch_view_on_face)
                : context.getString(R.string.sketch_view_label,
                                    context.getString(PLANE_LABELS[clampPlane(plane)]),
                                    normalName, quarterTurns * 90));
    }

    private static int clampPlane(int plane) {
        return plane < 0 || plane > 2 ? 0 : plane;
    }

    /** Which world axis a plane's normal runs along. */
    private static int planeNormalAxis(int plane) {
        switch (plane) {
            case NativeViewport.WORKPLANE_XZ:
                return R.string.axis_y;
            case NativeViewport.WORKPLANE_YZ:
                return R.string.axis_x;
            default:
                return R.string.axis_z;
        }
    }

    /**
     * Places the navigator in the viewport's upper trailing area, <b>clear of
     * the Tool Rail</b>.
     *
     * <p>The right contextual host runs down the trailing edge from just under
     * the Global Toolbar, so a control anchored to that corner would sit on top
     * of the rail's first entries — and a surface may stand on the model but
     * never on another live control. The navigator is therefore inset by the
     * host's own width plus a gap: still the upper-trailing corner of the
     * DRAWING, and never over anything that can be pressed.
     */
    static FrameLayout.LayoutParams anchoredParams(Context context) {
        final FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
        params.gravity = Gravity.TOP | Gravity.END;
        params.topMargin = EditorControlStyles.dimen(context, R.dimen.sketch_navigator_top_margin);
        params.rightMargin = EditorControlStyles.dimen(context, R.dimen.trailing_host_width)
                + EditorControlStyles.dimen(context, R.dimen.brush_gap)
                + EditorControlStyles.dimen(context, R.dimen.row_gap);
        return params;
    }

    /**
     * Caps the navigator's width so it stays a corner control.
     *
     * <p>Its widest row is three plane chips; left to wrap freely on a wide
     * window it would grow into a panel across the top of the drawing.
     */
    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        final int cap = EditorControlStyles.dimen(getContext(),
                R.dimen.sketch_navigator_max_width);
        final int available = MeasureSpec.getSize(widthMeasureSpec);
        final int wanted = available > 0 ? Math.min(cap, available) : cap;
        super.onMeasure(MeasureSpec.makeMeasureSpec(wanted, MeasureSpec.AT_MOST), heightMeasureSpec);
    }
}
