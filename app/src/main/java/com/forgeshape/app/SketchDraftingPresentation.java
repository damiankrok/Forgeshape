package com.forgeshape.app;

import java.math.BigDecimal;
import java.math.RoundingMode;

/**
 * The drafting chrome's presentation rules (`CAD-SKETCH-DRAFTING-TOOLKIT-E2E-R1`),
 * as pure Java over the native state so a JVM test pins every case.
 *
 * <ul>
 *   <li><b>The sketch actions palette</b> — which acts are drawn for the
 *       current selection. A control that cannot succeed is not drawn, so the
 *       list is the domain's own answer restated, never a menu of everything.</li>
 *   <li><b>Dimension labels</b> — {@code R} for a radius, {@code Ø} for a
 *       diameter, {@code °} for an angle, a Reference wrapped in parentheses;
 *       the value is DERIVED below JNI and only formatted here.</li>
 *   <li><b>Label collisions</b> — a label that would overlap one of higher
 *       priority is HIDDEN, never moved somewhere it does not belong.</li>
 *   <li><b>Snap feedback</b> — the spoken name of each snap kind, for the
 *       accessibility announcement; the viewport marker itself is a shape drawn
 *       below JNI and carries no text.</li>
 * </ul>
 */
final class SketchDraftingPresentation {

    private SketchDraftingPresentation() {
    }

    // -----------------------------------------------------------------------
    // The sketch actions palette
    // -----------------------------------------------------------------------

    static final int ACTION_SELECT_MULTIPLE = 0;
    static final int ACTION_DIMENSION = 1;
    static final int ACTION_CONSTRUCTION = 2;
    static final int ACTION_TRIM = 3;
    static final int ACTION_EXTEND = 4;
    static final int ACTION_OFFSET = 5;
    static final int ACTION_MIRROR = 6;
    static final int ACTION_DELETE = 7;
    static final int ACTION_COUNT = 8;

    /** The Modify entry exists while a sketch is DRAWN, and never in Ready. */
    static boolean modifyEntryShown(int sketchState) {
        return sketchState == NativeViewport.SKETCH_EDITING;
    }

    /** Whether an entity kind (SKETCH_ENTITY_KIND_*) has any dimension: all but a Spline. */
    static boolean kindDimensionable(int entityKind) {
        return entityKind >= NativeViewport.SKETCH_ENTITY_KIND_LINE
                && entityKind <= NativeViewport.SKETCH_ENTITY_KIND_ARC;
    }

    /** Whether an entity kind can be offset: all but a Spline. */
    static boolean kindOffsettable(int entityKind) {
        return kindDimensionable(entityKind);
    }

    /**
     * Which acts the palette draws, in reading order.
     *
     * <ul>
     *   <li>nothing selected: Select multiple, Dimension, Trim, Extend, Offset;</li>
     *   <li>ONE entity: Select multiple, Dimension and Offset (where its kind
     *       has them), Trim, Extend, Make Construction / Make Regular, Mirror,
     *       Delete;</li>
     *   <li>several: Select multiple, Trim, Extend, Make Construction / Make
     *       Regular, Mirror, Delete.</li>
     * </ul>
     *
     * <p>Trim and Extend are tap modes that act on what the finger touches,
     * never on the selection, so they are offered WHATEVER is selected: a
     * selection cannot make them fail, and every drawing tool leaves what it
     * just drew selected, so withdrawing them there would hide Trim exactly
     * when a user has finished drawing the lines to trim.
     */
    static boolean[] paletteActions(int sketchState, int selectionCount, int singleEntityKind) {
        final boolean[] shown = new boolean[ACTION_COUNT];
        if (!modifyEntryShown(sketchState)) {
            return shown;
        }
        shown[ACTION_SELECT_MULTIPLE] = true;
        shown[ACTION_TRIM] = true;
        shown[ACTION_EXTEND] = true;
        if (selectionCount <= 0) {
            shown[ACTION_DIMENSION] = true;
            shown[ACTION_OFFSET] = true;
            return shown;
        }
        if (selectionCount == 1) {
            shown[ACTION_DIMENSION] = kindDimensionable(singleEntityKind);
            shown[ACTION_OFFSET] = kindOffsettable(singleEntityKind);
        }
        shown[ACTION_CONSTRUCTION] = true;
        shown[ACTION_MIRROR] = true;
        shown[ACTION_DELETE] = true;
        return shown;
    }

    /** The construction act's label: Make Regular only when every selected entity is Construction. */
    static int constructionLabel(boolean targetIsConstruction) {
        return targetIsConstruction ? R.string.sketch_make_construction : R.string.sketch_make_regular;
    }

    // -----------------------------------------------------------------------
    // Dimension labels
    // -----------------------------------------------------------------------

    static boolean isAngle(int kind) {
        return kind == NativeViewport.DIM_LINE_ANGLE || kind == NativeViewport.DIM_ARC_SWEEP
                || kind == NativeViewport.DIM_EDGE_ANGLE;
    }

    /** The six kinds a Driving dimension may be (the native rule, restated). */
    static boolean drivingAllowed(int kind) {
        return kind == NativeViewport.DIM_LINE_LENGTH || kind == NativeViewport.DIM_LINE_ANGLE
                || kind == NativeViewport.DIM_RECTANGLE_WIDTH
                || kind == NativeViewport.DIM_RECTANGLE_HEIGHT
                || kind == NativeViewport.DIM_CIRCLE_RADIUS
                || kind == NativeViewport.DIM_CIRCLE_DIAMETER;
    }

    static boolean isRadius(int kind) {
        return kind == NativeViewport.DIM_CIRCLE_RADIUS || kind == NativeViewport.DIM_ARC_RADIUS;
    }

    /** An angle in degrees at the display precision, with the degree sign. */
    static String formatDegrees(double degrees) {
        return LengthUnit.present(BigDecimal.valueOf(degrees)
                .setScale(LengthUnit.DISPLAY_DECIMALS, RoundingMode.HALF_UP)) + "°";
    }

    /**
     * The label's text: {@code R 12.5 mm}, {@code Ø 25 mm}, {@code 45°},
     * {@code 40 mm}; a Reference wrapped in parentheses so driving and
     * reference read apart by more than colour.
     */
    static String label(int kind, boolean reference, double value, LengthUnit unit) {
        final String number;
        if (isAngle(kind)) {
            number = formatDegrees(value);
        } else if (isRadius(kind)) {
            number = "R " + unit.formatWithUnit(value);
        } else if (kind == NativeViewport.DIM_CIRCLE_DIAMETER) {
            number = "Ø " + unit.formatWithUnit(value);
        } else {
            number = unit.formatWithUnit(value);
        }
        return reference ? "(" + number + ")" : number;
    }

    /** The chip text for choosing a dimension kind. */
    static int kindName(int kind) {
        switch (kind) {
            case NativeViewport.DIM_LINE_LENGTH: return R.string.dimension_kind_length;
            case NativeViewport.DIM_LINE_ANGLE: return R.string.dimension_kind_angle;
            case NativeViewport.DIM_LINE_HORIZONTAL: return R.string.dimension_kind_horizontal;
            case NativeViewport.DIM_LINE_VERTICAL: return R.string.dimension_kind_vertical;
            case NativeViewport.DIM_RECTANGLE_WIDTH: return R.string.dimension_kind_width;
            case NativeViewport.DIM_RECTANGLE_HEIGHT: return R.string.dimension_kind_height;
            case NativeViewport.DIM_CIRCLE_RADIUS: return R.string.dimension_kind_radius;
            case NativeViewport.DIM_CIRCLE_DIAMETER: return R.string.dimension_kind_diameter;
            case NativeViewport.DIM_EDGE_LENGTH: return R.string.dimension_kind_edge_length;
            case NativeViewport.DIM_ARC_RADIUS: return R.string.dimension_kind_radius;
            case NativeViewport.DIM_ARC_SWEEP: return R.string.dimension_kind_sweep;
            case NativeViewport.DIM_EDGE_ANGLE: return R.string.dimension_kind_edge_angle;
            default: return R.string.dimension_kind_length;
        }
    }

    /**
     * The gap, in dp, kept between a label's touch box and the point on the
     * geometry it stands off from: the stroke and a finger's width beside it on
     * the label's side stay the drawing's.
     */
    static final float LABEL_STROKE_CLEAR_DP = 8.0f;

    /**
     * Where a label's touch box is centred: on the ray from {@code attach}
     * (the point on the measured geometry, see {@code SketchDimensionAnnotation})
     * through native's anchor, at the anchor's own distance or further -- just
     * far enough that the WHOLE {@code width} x {@code height} box clears
     * {@code attach} by {@code clearPx} along that ray. Native places the anchor
     * in reference units without knowing how wide the drawn number is, so a
     * label standing off a VERTICAL edge would otherwise reach back over the
     * edge by half its width. The box's reach back along the ray is its support
     * {@code (|dx| w + |dy| h) / 2}; for a straight edge the ray is square to
     * the edge, so clearing {@code attach} clears the whole edge's line.
     *
     * <p>A label is only ever pushed AWAY from what it measures, never sideways
     * and never back; with no honest direction (the anchor IS the attach point)
     * it stands at the anchor.
     */
    static float[] standOffCentre(float anchorX, float anchorY, float attachX, float attachY,
                                  float width, float height, float clearPx) {
        final float dx = anchorX - attachX;
        final float dy = anchorY - attachY;
        final float distance = (float) Math.hypot(dx, dy);
        if (!(distance > 0.5f) || !Float.isFinite(distance)) {
            return new float[] {anchorX, anchorY};
        }
        final float ux = dx / distance;
        final float uy = dy / distance;
        final float support = 0.5f * (Math.abs(ux) * width + Math.abs(uy) * height);
        final float stand = Math.max(distance, support + clearPx);
        return new float[] {attachX + ux * stand, attachY + uy * stand};
    }

    /**
     * Whether a box centred at {@code (cx, cy)} lies wholly on a viewport of
     * {@code viewportWidth} x {@code viewportHeight}. A label that does not is
     * HIDDEN rather than clamped in: a clamp moves it toward the middle of the
     * view, which can stand it back over the very geometry it was pushed off.
     */
    static boolean boxInside(float cx, float cy, float width, float height, int viewportWidth,
                             int viewportHeight) {
        return viewportWidth > 0 && viewportHeight > 0
                && cx - 0.5f * width >= 0.0f && cy - 0.5f * height >= 0.0f
                && cx + 0.5f * width <= viewportWidth && cy + 0.5f * height <= viewportHeight;
    }

    /**
     * Which labels stand, by priority: the higher priority claims its box
     * first and any later label overlapping a standing one is HIDDEN. Boxes are
     * centred on their anchors; {@code priority} higher wins and an exact tie
     * keeps the earlier index. Deterministic, and nothing is ever moved.
     */
    static boolean[] resolveVisible(float[] centreX, float[] centreY, float[] width, float[] height,
                                    int[] priority) {
        final int n = centreX.length;
        final boolean[] visible = new boolean[n];
        final Integer[] order = new Integer[n];
        for (int i = 0; i < n; i++) {
            order[i] = i;
        }
        java.util.Arrays.sort(order, (a, b) -> priority[a] != priority[b]
                ? Integer.compare(priority[b], priority[a]) : Integer.compare(a, b));
        for (int k = 0; k < n; k++) {
            final int i = order[k];
            boolean clear = true;
            for (int j = 0; j < n && clear; j++) {
                if (!visible[j]) {
                    continue;
                }
                final boolean overlapX = Math.abs(centreX[i] - centreX[j]) * 2.0f < width[i] + width[j];
                final boolean overlapY = Math.abs(centreY[i] - centreY[j]) * 2.0f < height[i] + height[j];
                clear = !(overlapX && overlapY);
            }
            visible[i] = clear;
        }
        return visible;
    }

    /**
     * A label's priority: the selected entity's own first, then Driving, then
     * the older (smaller) id.
     */
    static int priority(boolean ofSelection, boolean driving, long id) {
        final int idPart = (int) Math.max(0L, 0xFFFFL - Math.min(id, 0xFFFFL));
        return (ofSelection ? 1 << 18 : 0) | (driving ? 1 << 17 : 0) | idPart;
    }

    /**
     * The touch side of a label: never below the 48 dp floor, never larger
     * than the label itself where it is already larger, so a label does not
     * reach further into the drawing than the floor requires.
     */
    static int touchExtent(int measured, int floorPx) {
        return Math.max(measured, floorPx);
    }

    // -----------------------------------------------------------------------
    // Snap feedback
    // -----------------------------------------------------------------------

    /** The spoken name of a snap kind (SNAP_*), or 0 when there is none to say. */
    static int snapDescription(int snapKind) {
        switch (snapKind) {
            case NativeViewport.SNAP_ENDPOINT: return R.string.snap_endpoint;
            case NativeViewport.SNAP_INTERSECTION: return R.string.snap_intersection;
            case NativeViewport.SNAP_MIDPOINT: return R.string.snap_midpoint;
            case NativeViewport.SNAP_CENTER: return R.string.snap_center;
            case NativeViewport.SNAP_ORIGIN: return R.string.snap_origin;
            case NativeViewport.SNAP_HORIZONTAL_GUIDE: return R.string.snap_horizontal;
            case NativeViewport.SNAP_VERTICAL_GUIDE: return R.string.snap_vertical;
            case NativeViewport.SNAP_GRID: return R.string.snap_grid;
            default: return 0;
        }
    }
}
