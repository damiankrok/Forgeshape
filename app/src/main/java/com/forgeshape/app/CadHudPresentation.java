package com.forgeshape.app;

/**
 * The compact CAD extrude HUD's presentation rules, as plain values
 * (`CAD-VERTICAL-SLICE-R1`).
 *
 * <p><b>Why a separate class.</b> {@link CadExtrudeCanvasView} is a view and
 * can only be argued about on a device. The decisions it makes — how big a
 * glyph is drawn at a camera scale, how big the touch target is, which icon
 * and caption name a mode or an operation, which operations the badge offers,
 * where the cluster must stand so its VALUE lands on the arrow — are
 * arithmetic and lookup, and holding them here lets a JVM case pin every one
 * of them. The view asks; this answers; neither holds a domain value.
 *
 * <p><b>The glyph and the touch target are two different facts.</b> The glyph
 * follows the camera-attached multiplier native reports ({@code
 * CAD_EXTRUDE_SCALE}, 0.80..1.60), so the cluster still reads as belonging to
 * the work: {@code clamp(28 x scale, 24, 32)} dp. The hit area does NOT follow
 * it: every control is 48 dp at every scale. The previous cluster scaled whole
 * views by the multiplier, which scaled the hit area down with the glyph and
 * forced every control to be authored at 60 dp just so the smallest scale
 * still cleared the floor — a 60 dp text pill is what made the old cluster
 * span a phone's whole viewport.
 *
 * <p>Holds no Android type and reads no state; the {@code R} ids it returns
 * are plain integers.
 */
final class CadHudPresentation {

    /** The interactive floor, in dp: every HUD control's hit rectangle. */
    static final int HIT_DP = 48;
    /** The glyph at scale 1.0, in dp. */
    static final float GLYPH_BASE_DP = 28.0f;
    /** The smallest glyph, in dp: still legible on a pulled-back camera. */
    static final float GLYPH_MIN_DP = 24.0f;
    /**
     * The largest glyph, in dp. Below the 48 dp hit rectangle with room for
     * the pressed shape around it, so a close camera never grows a glyph into
     * its own control's edge.
     */
    static final float GLYPH_MAX_DP = 32.0f;
    /** The visible height of the exact value's pill, in dp (inside 48 dp). */
    static final int VALUE_PILL_DP = 32;

    /** The operations, in the native enum's order and the palette's order. */
    static final int[] OPERATIONS = {
            NativeViewport.OPERATION_NEW_BODY, NativeViewport.OPERATION_ADD,
            NativeViewport.OPERATION_CUT};
    /** The extent modes, in the native enum's order and the palette's order. */
    static final int[] EXTENTS = {
            NativeViewport.EXTENT_ONE_SIDE, NativeViewport.EXTENT_SYMMETRIC,
            NativeViewport.EXTENT_TWO_SIDES};

    private CadHudPresentation() {
    }

    // -----------------------------------------------------------------------
    // Size
    // -----------------------------------------------------------------------

    /**
     * The camera-attached multiplier as this HUD uses it.
     *
     * <p>Non-finite or non-positive is 1.0: a multiplier that says nothing is
     * treated as the reference size, never as zero or as a guess.
     */
    static float effectiveScale(double scale) {
        if (Double.isNaN(scale) || Double.isInfinite(scale) || scale <= 0.0) {
            return 1.0f;
        }
        return (float) scale;
    }

    /** The drawn glyph, in dp: {@code clamp(28 x scale, 24, 32)}. */
    static float glyphDp(double scale) {
        final float wanted = GLYPH_BASE_DP * effectiveScale(scale);
        return Math.max(GLYPH_MIN_DP, Math.min(GLYPH_MAX_DP, wanted));
    }

    /** The drawn glyph in pixels at a display density. At least one pixel. */
    static int glyphPx(double scale, float density) {
        return Math.max(1, Math.round(glyphDp(scale) * density));
    }

    /**
     * The hit rectangle's side, in dp. Deliberately takes the scale and
     * ignores it: that is the whole rule, stated where a case can hold it.
     */
    static int hitDp(double scale) {
        return HIT_DP;
    }

    /** The hit rectangle's side in pixels at a display density. */
    static int hitPx(float density) {
        return Math.round(HIT_DP * density);
    }

    // -----------------------------------------------------------------------
    // Extent
    // -----------------------------------------------------------------------

    /** The glyph for an extent mode. An unknown mode reads as One Side. */
    static int extentIcon(int mode) {
        switch (mode) {
            case NativeViewport.EXTENT_SYMMETRIC:
                return R.drawable.ic_extent_symmetric;
            case NativeViewport.EXTENT_TWO_SIDES:
                return R.drawable.ic_extent_two_sides;
            default:
                return R.drawable.ic_extent_one_side;
        }
    }

    /** The one-word caption for an extent mode. */
    static int extentCaption(int mode) {
        switch (mode) {
            case NativeViewport.EXTENT_SYMMETRIC:
                return R.string.cad_hud_caption_symmetric;
            case NativeViewport.EXTENT_TWO_SIDES:
                return R.string.cad_hud_caption_two_sides;
            default:
                return R.string.cad_hud_caption_one_side;
        }
    }

    /** What an extent mode does, in one sentence, for its accessible name. */
    static int extentDescription(int mode) {
        switch (mode) {
            case NativeViewport.EXTENT_SYMMETRIC:
                return R.string.extent_symmetric_description;
            case NativeViewport.EXTENT_TWO_SIDES:
                return R.string.extent_two_sides_description;
            default:
                return R.string.extent_one_side_description;
        }
    }

    /**
     * Whether Flip is drawn. One Side ALONE: Symmetric already reaches both
     * sides and Two Sides states both, so there is no side left to choose and
     * the control is absent rather than shown and refused.
     */
    static boolean flipPresent(int mode) {
        return mode == NativeViewport.EXTENT_ONE_SIDE;
    }

    /**
     * Whether the second side's own value is drawn. Two Sides alone: Symmetric
     * draws two arrows and ONE distance, and a second number would be the same
     * value written twice.
     */
    static boolean secondValuePresent(int mode) {
        return mode == NativeViewport.EXTENT_TWO_SIDES;
    }

    // -----------------------------------------------------------------------
    // Operation
    // -----------------------------------------------------------------------

    /** Whether a value is one of the three operations native names. */
    static boolean isKnownOperation(int operation) {
        return operation == NativeViewport.OPERATION_NEW_BODY
                || operation == NativeViewport.OPERATION_ADD
                || operation == NativeViewport.OPERATION_CUT;
    }

    /** The {@code OPERATION_BIT_*} for an operation, or 0 for an unknown one. */
    static int operationBit(int operation) {
        switch (operation) {
            case NativeViewport.OPERATION_NEW_BODY:
                return NativeViewport.OPERATION_BIT_NEW_BODY;
            case NativeViewport.OPERATION_ADD:
                return NativeViewport.OPERATION_BIT_ADD;
            case NativeViewport.OPERATION_CUT:
                return NativeViewport.OPERATION_BIT_CUT;
            default:
                return 0;
        }
    }

    /** Whether native says this operation can be chosen now. */
    static boolean operationOffered(int operation, int availableBits) {
        final int bit = operationBit(operation);
        return bit != 0 && (availableBits & bit) != 0;
    }

    /** How many of the three operations the bitmask offers. */
    static int offeredCount(int availableBits) {
        int count = 0;
        for (int operation : OPERATIONS) {
            if (operationOffered(operation, availableBits)) {
                count++;
            }
        }
        return count;
    }

    /**
     * Whether the operation badge is a CONTROL: true only when the palette it
     * opens would hold something other than the operation already in force.
     *
     * <p>When only New Body can be made the badge still stands — it says what
     * this extrusion does — but it is a statement, not a control, because a
     * palette offering exactly the current answer is a choice with nothing to
     * choose.
     */
    static boolean operationBadgeHasChoice(int current, int availableBits) {
        final int others = offeredCount(availableBits)
                - (operationOffered(current, availableBits) ? 1 : 0);
        return others > 0;
    }

    /** The glyph for an operation. An unknown one reads as New Body. */
    static int operationIcon(int operation) {
        switch (operation) {
            case NativeViewport.OPERATION_ADD:
                return R.drawable.ic_op_add;
            case NativeViewport.OPERATION_CUT:
                return R.drawable.ic_op_cut;
            default:
                return R.drawable.ic_op_new_body;
        }
    }

    /** The one-word caption for an operation. */
    static int operationCaption(int operation) {
        switch (operation) {
            case NativeViewport.OPERATION_ADD:
                return R.string.cad_hud_caption_add;
            case NativeViewport.OPERATION_CUT:
                return R.string.cad_hud_caption_cut;
            default:
                return R.string.cad_hud_caption_new_body;
        }
    }

    /** What an operation does, in one sentence, for its accessible name. */
    static int operationDescription(int operation) {
        switch (operation) {
            case NativeViewport.OPERATION_ADD:
                return R.string.cad_hud_operation_add_description;
            case NativeViewport.OPERATION_CUT:
                return R.string.cad_hud_operation_cut_description;
            default:
                return R.string.cad_hud_operation_new_body_description;
        }
    }

    /**
     * The theme ROLE an operation's glyph is tinted with — never the only
     * carrier of the meaning, because the glyph's SHAPE (a lone solid, a boss
     * and a plus, a notch and a minus) and the accessible name carry it too.
     *
     * <p>Add is the palette's success role and Cut its error role, which every
     * palette already holds apart from each other and from body text. New Body
     * is the primary content role rather than {@code fsAccent}: attrs.xml
     * spends the accent on exactly two things, a primary commit and a focused
     * field's ring, and a badge wearing it would read as a third commit button
     * beside the value.
     */
    static int operationColorAttr(int operation) {
        switch (operation) {
            case NativeViewport.OPERATION_ADD:
                return R.attr.fsTextSuccess;
            case NativeViewport.OPERATION_CUT:
                return R.attr.fsTextError;
            default:
                return R.attr.fsTextPrimary;
        }
    }

    // -----------------------------------------------------------------------
    // Placement
    // -----------------------------------------------------------------------

    /**
     * How far the cluster's centre must stand from the anchor so that the
     * VALUE's centre lands on it.
     *
     * <p>{@link ViewportAnchorSpace} centres whatever box it is given on the
     * anchor. The anchor is the arrow shaft's midpoint and the value is the
     * number that measures that shaft, so it is the value — not the cluster —
     * that belongs there; the extent button stands to its left and the badge
     * and Flip to its right. Passing {@code anchor + offset} to the shared
     * placement puts the value's centre exactly on the anchor, and the shared
     * clamp still keeps the whole cluster inside the viewport.
     *
     * @param clusterWidth the cluster's measured width
     * @param valueStart   the value's left edge inside the cluster
     * @param valueWidth   the value's measured width
     */
    static float valueCentreOffset(int clusterWidth, int valueStart, int valueWidth) {
        return clusterWidth * 0.5f - (valueStart + valueWidth * 0.5f);
    }

    /**
     * The start of a box of {@code size} centred on {@code centre}, kept inside
     * {@code [0, extent]} — the same clamp {@link ViewportAnchorSpace#place}
     * applies, stated in viewport pixels so a palette can be hung from the box
     * the cluster was actually given rather than from the one it asked for.
     * A box larger than the extent starts at 0, as there.
     */
    static float clampedStart(float centre, float size, float extent) {
        final float max = extent - size;
        if (max < 0.0f) {
            return 0.0f;
        }
        return Math.max(0.0f, Math.min(centre - size * 0.5f, max));
    }

    /**
     * Where an open palette's centre stands vertically: directly BELOW the
     * cluster when it fits in the viewport, otherwise directly ABOVE it, and
     * when neither fits, on whichever side has more room (the shared clamp
     * then keeps it on screen).
     */
    static float paletteCentreY(float clusterTop, float clusterHeight, float paletteHeight,
                                float gap, float viewportHeight) {
        final float belowTop = clusterTop + clusterHeight + gap;
        final float aboveBottom = clusterTop - gap;
        final boolean fitsBelow = belowTop + paletteHeight <= viewportHeight;
        final boolean fitsAbove = aboveBottom - paletteHeight >= 0.0f;
        if (fitsBelow || (!fitsAbove && viewportHeight - belowTop >= aboveBottom)) {
            return belowTop + paletteHeight * 0.5f;
        }
        return aboveBottom - paletteHeight * 0.5f;
    }
}
