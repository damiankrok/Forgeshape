package com.forgeshape.app;

/**
 * The CAD extrude HUD's presentation rules, as plain values
 * (`CAD-VERTICAL-SLICE-R1`, reshaped as a technical-drawing annotation by
 * `CAD-FOUNDATION-C1`).
 *
 * <p><b>Why a separate class.</b> {@link CadExtrudeCanvasView} is a view and
 * can only be argued about on a device. The decisions it makes — how big a
 * glyph and the value text are drawn at a camera scale, how big the invisible
 * touch proxy is, where along the projected leader each control stands, which
 * way the value reads, which icon and caption name a mode or an operation,
 * which operations the badge offers — are arithmetic and lookup, and holding
 * them here lets a JVM case pin every one of them. The view asks; this answers;
 * neither holds a domain value, and nothing here projects: every screen point
 * it reasons about was projected below JNI.
 *
 * <p><b>What is drawn and what is touched are two facts.</b> The visible glyph
 * and the value's text follow the camera-attached multiplier native reports
 * ({@code CAD_EXTRUDE_SCALE}, 0.40..1.60): {@code 28 dp x scale} for a glyph and
 * {@code clamp(14 sp x scale, 11, 18)} for the value. The touch target never
 * follows it: every control is an INVISIBLE proxy of at least 48 dp, centred on
 * the drawn glyph, and it paints nothing, so a pulled-back camera shows a small
 * annotation on the work rather than a row of 48 dp capsules.
 *
 * <p><b>The value belongs to its leader.</b> Native projects the dimension line
 * the frame draws beside the shaft; the value stands ABOVE that line, rotated to
 * it and kept upright, at the middle of the part of the line that is actually on
 * screen. The extent control stands on the leader's line past its base end, the
 * operation badge and Flip past its tip end, each far enough along that no two
 * proxies overlap.
 *
 * <p><b>OWNER-TUNABLE.</b> Every size constant below is a provisional
 * presentation choice with stated arithmetic, not architecture; the
 * physical-device review decides the final values.
 *
 * <p>Holds no Android type and reads no state; the {@code R} ids it returns
 * are plain integers.
 */
final class CadHudPresentation {

    /** The interactive floor, in dp: every HUD control's invisible touch proxy. */
    static final int HIT_DP = 48;
    /** The drawn glyph at scale 1.0, in dp. OWNER-TUNABLE. */
    static final float GLYPH_BASE_DP = 28.0f;
    /**
     * The visual scale band the glyph and the value text follow — the native
     * band, restated so a JVM case can hold it: 0.40 is a glyph of about 11 dp
     * on a pulled-back camera, 1.60 one of about 45 dp up close. OWNER-TUNABLE.
     */
    static final float VISUAL_SCALE_MIN = 0.40f;
    static final float VISUAL_SCALE_MAX = 1.60f;
    /** The value text at scale 1.0, and its legibility band, in sp. OWNER-TUNABLE. */
    static final float VALUE_TEXT_BASE_SP = 14.0f;
    static final float VALUE_TEXT_MIN_SP = 11.0f;
    static final float VALUE_TEXT_MAX_SP = 18.0f;
    /** How far the value's text stands above its leader, in dp. */
    static final float VALUE_GAP_DP = 3.0f;
    /** The gap between a glyph and the end of the leader it stands past, in dp. */
    static final float GLYPH_GAP_DP = 4.0f;
    /**
     * The retained-sketch chip's glyph band, in dp. That chip is a lone control
     * standing on a committed body, not a drawing annotation, and it keeps the
     * compact band it was approved with.
     */
    static final float LONE_GLYPH_MIN_DP = 24.0f;
    static final float LONE_GLYPH_MAX_DP = 32.0f;

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

    /** The multiplier clamped into the visual band. */
    static float visualScale(double scale) {
        final float s = effectiveScale(scale);
        return Math.max(VISUAL_SCALE_MIN, Math.min(VISUAL_SCALE_MAX, s));
    }

    /** The drawn glyph, in dp: {@code 28 x clamp(scale, 0.40, 1.60)}. */
    static float glyphDp(double scale) {
        return GLYPH_BASE_DP * visualScale(scale);
    }

    /** The drawn glyph in pixels at a display density. At least one pixel. */
    static int glyphPx(double scale, float density) {
        return Math.max(1, Math.round(glyphDp(scale) * density));
    }

    /** The lone retained-sketch chip's glyph, in dp: {@code clamp(28 x scale, 24, 32)}. */
    static float loneGlyphDp(double scale) {
        final float wanted = GLYPH_BASE_DP * effectiveScale(scale);
        return Math.max(LONE_GLYPH_MIN_DP, Math.min(LONE_GLYPH_MAX_DP, wanted));
    }

    static int loneGlyphPx(double scale, float density) {
        return Math.max(1, Math.round(loneGlyphDp(scale) * density));
    }

    /** The value's text size, in sp: {@code clamp(14 x scale, 11, 18)}. */
    static float valueTextSp(double scale) {
        final float wanted = VALUE_TEXT_BASE_SP * effectiveScale(scale);
        return Math.max(VALUE_TEXT_MIN_SP, Math.min(VALUE_TEXT_MAX_SP, wanted));
    }

    /**
     * Whether a control shows its Tool Labels caption. A palette choice does
     * when the preference is on; a glyph attached to the leader never does,
     * because it shrinks with the work and a fixed-size word under a shrinking
     * mark would be neither drawing nor chrome.
     */
    static boolean captionShown(boolean toolLabels, boolean attached) {
        return toolLabels && !attached;
    }

    /**
     * The touch proxy's side, in dp. Deliberately takes the scale and ignores
     * it: that is the whole rule, stated where a case can hold it.
     */
    static int hitDp(double scale) {
        return HIT_DP;
    }

    /** The touch proxy's side in pixels at a display density. */
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
     * The upright reading angle of a screen direction, in degrees within
     * {@code [-90, 90)}: the direction itself when it points right (or, when
     * exactly vertical, up the screen), otherwise the reverse. Text rotated by it
     * never reads upside down, and a vertical leader reads bottom to top — the
     * drawing convention. Native signs the leader's side with the SAME rule, so
     * "above the leader" is always away from the shaft. A zero vector reads 0.
     */
    static float readingAngleDegrees(float dx, float dy) {
        if (dx == 0.0f && dy == 0.0f) {
            return 0.0f;
        }
        if (dx < 0.0f || (dx == 0.0f && dy > 0.0f)) {
            dx = -dx;
            dy = -dy;
        }
        final float degrees = (float) Math.toDegrees(Math.atan2(dy, dx));
        return degrees >= 90.0f ? degrees - 180.0f : degrees;
    }

    /**
     * The part of the segment {@code (x0,y0)-(x1,y1)} inside {@code [0,w] x
     * [0,h]} (Liang–Barsky), as {@code {x0, y0, x1, y1}}, or null when none of it
     * is. The value stands on the visible part of its leader, so a leader running
     * off the screen still has its value where the user can read and tap it —
     * and a leader wholly off screen has no value at all rather than one clamped
     * to an edge at a guess.
     */
    static float[] clipToViewport(float x0, float y0, float x1, float y1, float w, float h) {
        if (!(w > 0.0f) || !(h > 0.0f) || Float.isNaN(x0) || Float.isNaN(y0)
                || Float.isNaN(x1) || Float.isNaN(y1)) {
            return null;
        }
        final float dx = x1 - x0;
        final float dy = y1 - y0;
        final float[] p = {-dx, dx, -dy, dy};
        final float[] q = {x0, w - x0, y0, h - y0};
        float t0 = 0.0f;
        float t1 = 1.0f;
        for (int i = 0; i < 4; i++) {
            if (p[i] == 0.0f) {
                if (q[i] < 0.0f) {
                    return null;
                }
                continue;
            }
            final float t = q[i] / p[i];
            if (p[i] < 0.0f) {
                if (t > t1) {
                    return null;
                }
                t0 = Math.max(t0, t);
            } else {
                if (t < t0) {
                    return null;
                }
                t1 = Math.min(t1, t);
            }
        }
        return new float[]{x0 + dx * t0, y0 + dy * t0, x0 + dx * t1, y0 + dy * t1};
    }

    /**
     * Where everything attached to one leader stands, in viewport pixels.
     *
     * <p>{@code valueVisible} false means the leader is wholly off screen and
     * every attached control is hidden with it. A glyph whose own centre falls
     * outside the viewport is hidden alone, never clamped back onto the screen
     * away from the leader it belongs to.
     */
    static final class LeaderLayout {
        boolean valueVisible;
        float valueX;
        float valueY;
        /** The value's rotation, degrees, upright. */
        float rotation;
        boolean extentVisible;
        float extentX;
        float extentY;
        boolean operationVisible;
        float operationX;
        float operationY;
        boolean flipVisible;
        float flipX;
        float flipY;
    }

    /**
     * Lays one leader's controls out.
     *
     * @param sx          the leader's start (beside the base), viewport px
     * @param sy          the same, vertically
     * @param ex          the leader's end (beside the tip), viewport px
     * @param ey          the same, vertically
     * @param viewportW   the viewport width, px
     * @param viewportH   the viewport height, px
     * @param valueW      the value proxy's measured width, px (at least the hit side)
     * @param valueH      the value proxy's measured height, px
     * @param textH       the value TEXT's own height, px (what stands above the line)
     * @param glyphPx     the drawn glyph side, px
     * @param hitPx       the touch proxy side, px
     * @param gapPx       the glyph gap, px
     * @param valueGapPx  the gap between the leader and the value text, px
     * @param withGlyphs  whether the extent and operation controls stand here
     * @param flipPresent whether Flip stands here (One Side only)
     * @param extentPastTip whether the extent control stands past the TIP end,
     *                    in Flip's place, rather than past the base end — true
     *                    whenever the extrusion reaches the other side too
     *                    (Symmetric, Two Sides), because past the base end is
     *                    then the OTHER side's leader
     */
    static LeaderLayout layoutLeader(float sx, float sy, float ex, float ey, float viewportW,
                                     float viewportH, float valueW, float valueH, float textH,
                                     float glyphPx, float hitPx, float gapPx, float valueGapPx,
                                     boolean withGlyphs, boolean flipPresent,
                                     boolean extentPastTip) {
        final LeaderLayout out = new LeaderLayout();
        final float[] visible = clipToViewport(sx, sy, ex, ey, viewportW, viewportH);
        if (visible == null) {
            return out;
        }
        float dx = ex - sx;
        float dy = ey - sy;
        final float length = (float) Math.hypot(dx, dy);
        if (length > 1.0e-3f) {
            dx /= length;
            dy /= length;
        } else {
            // Seen end-on: the leader is a point. Lay out horizontally about it.
            dx = 1.0f;
            dy = 0.0f;
        }
        out.rotation = readingAngleDegrees(dx, dy);
        final double radians = Math.toRadians(out.rotation);
        final float rx = (float) Math.cos(radians);
        final float ry = (float) Math.sin(radians);
        // "Up" for upright text along the reading direction, y-down screen.
        final float ux = ry;
        final float uy = -rx;
        final float mx = (visible[0] + visible[2]) * 0.5f;
        final float my = (visible[1] + visible[3]) * 0.5f;
        final float above = textH * 0.5f + valueGapPx;
        out.valueX = mx + ux * above;
        out.valueY = my + uy * above;
        out.valueVisible = true;
        if (!withGlyphs) {
            return out;
        }
        // Along the leader, measured from the VISIBLE midpoint: far enough past
        // each end to clear the drawn line, and far enough from the value that
        // two proxies never overlap -- the value's rotated box projects half its
        // width along the line, a hit square at most `hit/2 x (|rx| + |ry|)`.
        final float squareHalfAlong = hitPx * 0.5f * (Math.abs(rx) + Math.abs(ry));
        final float half = (float) Math.hypot(visible[2] - visible[0], visible[3] - visible[1])
                * 0.5f;
        final float clearOfLine = half + gapPx + glyphPx * 0.5f;
        final float clearOfValue = valueW * 0.5f + squareHalfAlong;
        final float reach = Math.max(clearOfLine, clearOfValue);
        // Two squares side by side along a direction clear each other once the
        // larger of their x and y separations reaches the side.
        final float step = Math.max(glyphPx + gapPx,
                hitPx / Math.max(Math.abs(dx), Math.abs(dy)));
        out.operationX = mx + dx * reach;
        out.operationY = my + dy * reach;
        out.flipX = out.operationX + dx * step;
        out.flipY = out.operationY + dy * step;
        if (extentPastTip) {
            out.extentX = out.flipX;
            out.extentY = out.flipY;
        } else {
            out.extentX = mx - dx * reach;
            out.extentY = my - dy * reach;
        }
        out.extentVisible = inside(out.extentX, out.extentY, viewportW, viewportH);
        out.operationVisible = inside(out.operationX, out.operationY, viewportW, viewportH);
        out.flipVisible = flipPresent && inside(out.flipX, out.flipY, viewportW, viewportH);
        return out;
    }

    private static boolean inside(float x, float y, float w, float h) {
        return x >= 0.0f && y >= 0.0f && x <= w && y <= h;
    }

    /**
     * The axis-aligned box a {@code w x h} box rotated by {@code degrees}
     * occupies, as {@code {width, height}} — what a rotated value covers on
     * screen, for the clamp that keeps it inside the viewport.
     */
    static float[] rotatedBounds(float w, float h, float degrees) {
        final double r = Math.toRadians(degrees);
        final float c = (float) Math.abs(Math.cos(r));
        final float s = (float) Math.abs(Math.sin(r));
        return new float[]{w * c + h * s, w * s + h * c};
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
