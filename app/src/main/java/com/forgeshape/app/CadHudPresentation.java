package com.forgeshape.app;

/**
 * The CAD extrude HUD's presentation rules, as plain values
 * (`CAD-VERTICAL-SLICE-R1`, reshaped as a technical-drawing annotation by
 * `CAD-FOUNDATION-C1`, and its controls gathered into ONE action panel by
 * `CAD-FOUNDATION-C2`).
 *
 * <p><b>Why a separate class.</b> {@link CadExtrudeCanvasView} is a view and
 * can only be argued about on a device. The decisions it makes — how big the
 * panel and the value text are drawn at a camera scale, how big the invisible
 * touch proxy is, where the panel stands against the arrow, when the whole
 * annotation collapses, which way the value reads, which icon and caption name
 * a mode or an operation, which operations the palette offers — are arithmetic
 * and lookup, and holding them here lets a JVM case pin every one of them. The
 * view asks; this answers; neither holds a domain value, and nothing here
 * projects: every screen point it reasons about was projected below JNI.
 *
 * <p><b>Two annotations, one scale.</b> The exact VALUE stands above the
 * dimension leader native draws beside the shaft, rotated to it and kept
 * upright. The extent, the operation and (One Side only) Flip are ONE compact
 * ACTION PANEL: a rigid plate of glyphs laid out once at its reference size and
 * scaled as a unit, anchored just past the arrow's drawn point (the head). Both
 * follow the ONE camera-attached multiplier native reports
 * ({@code CAD_EXTRUDE_SCALE}, 0.40..1.60): the panel's every length is
 * {@code reference x scale}, and the value's text is {@code 14 sp x scale}
 * inside a legibility band. The panel is shown WHOLE or not at all — it never
 * loses one icon to the screen edge and never spreads its icons apart.
 *
 * <p><b>What is drawn and what is touched are two facts.</b> The plate is a
 * drawing and takes no touch. The panel's touch target is ONE invisible group
 * proxy, at least 48 dp each way and covering the whole plate, which opens the
 * action palette — ordinary, readable screen chrome holding every choice at
 * its full size. At the scales the plate is drawn at, three 48 dp proxies
 * centred on three glyphs would overlap one another (the glyph pitch is 30 dp
 * at scale 1), and spreading the glyphs to clear them is exactly the scatter
 * this panel replaces; so a touch never has to choose between icons.
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
    /**
     * The value text at scale 1.0, and its legibility band, in sp. The text
     * follows the SAME visual scale as the panel; the band only keeps it
     * readable at the far end and bounded at the near one. OWNER-TUNABLE.
     */
    static final float VALUE_TEXT_BASE_SP = 14.0f;
    static final float VALUE_TEXT_MIN_SP = 9.0f;
    static final float VALUE_TEXT_MAX_SP = 18.0f;
    /** How far the value's text stands above its leader, in dp. */
    static final float VALUE_GAP_DP = 3.0f;
    /** The action panel's gap between two glyphs, at scale 1.0, in dp. OWNER-TUNABLE. */
    static final float PANEL_GAP_DP = 2.0f;
    /** The action panel's plate padding around its glyphs, at scale 1.0, in dp. */
    static final float PANEL_PAD_DP = 4.0f;
    /**
     * The extrude arrow's grab corridor, in dp — {@code kCadExtrudeGrabRadiusUnits}
     * restated. The panel's touch proxy stays at least this far (plus
     * {@link #PANEL_CLEAR_DP}) from the arrow's drawn point, so the panel never
     * takes a touch that would have grabbed the arrow.
     */
    static final float ARROW_CORRIDOR_DP = 24.0f;
    /** The extra gap between the arrow's corridor and the panel's proxy, in dp. */
    static final float PANEL_CLEAR_DP = 4.0f;
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

    /**
     * The value's text size, in sp: {@code clamp(14 x visualScale, 9, 18)} —
     * the panel's own multiplier, so the value and the panel grow and shrink
     * together.
     */
    static float valueTextSp(double scale) {
        final float wanted = VALUE_TEXT_BASE_SP * visualScale(scale);
        return Math.max(VALUE_TEXT_MIN_SP, Math.min(VALUE_TEXT_MAX_SP, wanted));
    }

    /**
     * Whether a control shows its Tool Labels caption. A palette choice does
     * when the preference is on; a glyph on the action panel never does,
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
     * Where one leader's value stands, in viewport pixels.
     *
     * <p>{@code valueVisible} false means the leader is wholly off screen and
     * its value is hidden with it. {@code visibleLength} is how much of the
     * leader is on screen, in pixels — what {@link #annotationCollapsed}
     * compares the value's own width with.
     */
    static final class LeaderLayout {
        boolean valueVisible;
        float valueX;
        float valueY;
        /** The value's rotation, degrees, upright. */
        float rotation;
        /** The on-screen length of the leader, px. */
        float visibleLength;
    }

    /**
     * Lays one leader's value out: above the middle of the leader's VISIBLE
     * part, rotated to the leader and upright.
     *
     * @param sx         the leader's start (beside the base), viewport px
     * @param sy         the same, vertically
     * @param ex         the leader's end (beside the tip), viewport px
     * @param ey         the same, vertically
     * @param viewportW  the viewport width, px
     * @param viewportH  the viewport height, px
     * @param textH      the value TEXT's own height, px (what stands above the line)
     * @param valueGapPx the gap between the leader and the value text, px
     */
    static LeaderLayout layoutLeader(float sx, float sy, float ex, float ey, float viewportW,
                                     float viewportH, float textH, float valueGapPx) {
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
            // Seen end-on: the leader is a point. Read horizontally about it.
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
        out.visibleLength = (float) Math.hypot(visible[2] - visible[0], visible[3] - visible[1]);
        out.valueVisible = true;
        return out;
    }

    /**
     * Whether the whole annotation — the value(s) and the action panel —
     * collapses (`CAD-FOUNDATION-C2`).
     *
     * <p>Only when BOTH hold: native reports the camera scale clamped at its
     * FLOOR (the annotation has stopped shrinking with the work), and the
     * leader's visible length is shorter than the value's own text — so the
     * number would be bigger than the dimension it states. Either alone is not
     * enough: a large model at its floor still has a long leader to stand on,
     * and a thin extrusion seen up close has a short leader but a scale that is
     * still shrinking with the camera. Collapsing hides the annotation WHOLE;
     * the arrow stays (native draws it), and the exact fields stay one tap away
     * on the precision surface.
     */
    static boolean annotationCollapsed(boolean clampedAtFloor, float leaderVisibleLength,
                                       float valueTextWidth) {
        return clampedAtFloor && leaderVisibleLength < valueTextWidth;
    }

    // -----------------------------------------------------------------------
    // The action panel
    // -----------------------------------------------------------------------

    /** The panel stands past the arrow's point, along the arrow. */
    static final int PANEL_BEYOND = 1;
    /** The panel stands beside the point, on the side AWAY from the leader. */
    static final int PANEL_AWAY = 2;
    /** The panel stands beside the point, on the leader's side. */
    static final int PANEL_TOWARD = 3;

    /** How many glyphs the panel holds: extent and operation, plus Flip in One Side. */
    static int panelIconCount(int extentMode) {
        return flipPresent(extentMode) ? 3 : 2;
    }

    /** The plate's width at scale 1.0, in dp: the glyphs, their gaps and the padding. */
    static float panelReferenceWidthDp(int iconCount) {
        final int n = Math.max(1, iconCount);
        return n * GLYPH_BASE_DP + (n - 1) * PANEL_GAP_DP + 2.0f * PANEL_PAD_DP;
    }

    /** The plate's height at scale 1.0, in dp. */
    static float panelReferenceHeightDp() {
        return GLYPH_BASE_DP + 2.0f * PANEL_PAD_DP;
    }

    /**
     * The centre of glyph {@code index} relative to the plate's centre, along
     * the plate, in dp at the given scale. Every glyph's offset is its
     * reference offset times the ONE visual scale, so the ratio between any two
     * spacings — and between a spacing and a glyph — never changes with zoom.
     */
    static float panelIconOffsetDp(int index, int iconCount, double scale) {
        final float pitch = GLYPH_BASE_DP + PANEL_GAP_DP;
        return (index - (iconCount - 1) * 0.5f) * pitch * visualScale(scale);
    }

    /** Where the action panel stands this frame, in viewport pixels. */
    static final class PanelLayout {
        /** False: the panel is hidden WHOLE this frame. */
        boolean visible;
        /** {@link #PANEL_BEYOND}, {@link #PANEL_AWAY}, {@link #PANEL_TOWARD}, or 0. */
        int placement;
        /** The plate's (and the proxy's) common centre. */
        float centreX;
        float centreY;
        /** The drawn plate, scale folded in. */
        float plateWidth;
        float plateHeight;
        /** The touch proxy: at least the hit floor, and never smaller than the plate. */
        float hitWidth;
        float hitHeight;
        /** The one visual multiplier the plate is drawn at. */
        float scale;
        /** The arrow point the panel is anchored to. */
        float anchorX;
        float anchorY;
    }

    /**
     * Lays the action panel out against the arrow's drawn point.
     *
     * <p>The panel is one box: a plate of {@code reference x scale} and a touch
     * proxy of {@code max(plate, 48 dp)} sharing one centre. It stands past the
     * point along the arrow's screen direction, with the proxy's near edge a
     * clear corridor away from the point; if that box would leave the viewport
     * it tries beside the point, away from the leader first, then on the
     * leader's side, each allowed to slide back along the shaft by at most its
     * own length (so it still stands level with the point, and still a clear
     * corridor off the arrow); if no whole box fits, the panel is hidden
     * WHOLE. It is never clamped onto the screen away from the arrow and never
     * loses an icon to an edge.
     *
     * @param headOnScreen whether native projected the arrow's point
     * @param headX        the arrow's drawn point, viewport px
     * @param headY        the same, vertically
     * @param axisX        the arrow's screen direction (any length), x
     * @param axisY        the same, y; a near-zero direction reads as screen right
     * @param leaderX      a point on the leader, to tell its side; NaN if none
     * @param leaderY      the same, vertically
     * @param plateWidth   the plate's MEASURED width at scale 1.0, px — the view's
     *                     own pixel-rounded box, so the box reasoned about here
     *                     is the box drawn ({@link #panelReferenceWidthDp} x
     *                     density up to rounding)
     * @param plateHeight  the same, vertically
     * @param scale        native's camera-attached multiplier
     * @param density      display density, px per dp
     * @param viewportW    the viewport width, px
     * @param viewportH    the viewport height, px
     */
    static PanelLayout layoutPanel(boolean headOnScreen, float headX, float headY, float axisX,
                                   float axisY, float leaderX, float leaderY, float plateWidth,
                                   float plateHeight, double scale, float density,
                                   float viewportW, float viewportH) {
        final PanelLayout out = new PanelLayout();
        out.scale = visualScale(scale);
        out.plateWidth = plateWidth * out.scale;
        out.plateHeight = plateHeight * out.scale;
        // Whole pixels, rounded UP: the proxy is a view with an integer box,
        // and it must never be a pixel short of the plate it covers.
        final float hit = HIT_DP * density;
        out.hitWidth = (float) Math.ceil(Math.max(hit, out.plateWidth));
        out.hitHeight = (float) Math.ceil(Math.max(hit, out.plateHeight));
        out.anchorX = headX;
        out.anchorY = headY;
        if (!headOnScreen || Float.isNaN(headX) || Float.isNaN(headY)
                || !inside(headX, headY, viewportW, viewportH)) {
            return out;
        }
        float dx = axisX;
        float dy = axisY;
        final float length = (float) Math.hypot(dx, dy);
        if (!(length > 1.0f)) {
            dx = 1.0f;
            dy = 0.0f;
        } else {
            dx /= length;
            dy /= length;
        }
        // The perpendicular pointing AWAY from the leader; with no leader,
        // the one a quarter turn clockwise on a y-down screen, deterministically.
        float px = -dy;
        float py = dx;
        if (!Float.isNaN(leaderX) && !Float.isNaN(leaderY)
                && ((leaderX - headX) * px + (leaderY - headY) * py) > 0.0f) {
            px = -px;
            py = -py;
        }
        final float clear = (ARROW_CORRIDOR_DP + PANEL_CLEAR_DP) * density;
        final float[][] candidates = {{dx, dy}, {px, py}, {-px, -py}};
        final int[] names = {PANEL_BEYOND, PANEL_AWAY, PANEL_TOWARD};
        // How far a BESIDE placement may slide back along the shaft: at most
        // the box's own extent along the arrow, so some of it still stands
        // level with the point and it reads as belonging to the tip. Sliding
        // along the arrow keeps the perpendicular clearance, so the box never
        // enters the arrow's grab corridor.
        final float alongExtent = out.hitWidth * Math.abs(dx) + out.hitHeight * Math.abs(dy);
        for (int i = 0; i < candidates.length; i++) {
            final float ux = candidates[i][0];
            final float uy = candidates[i][1];
            // Half the proxy box's extent along u: its support in that direction.
            final float half = 0.5f * (out.hitWidth * Math.abs(ux) + out.hitHeight * Math.abs(uy));
            final float baseX = headX + ux * (clear + half);
            final float baseY = headY + uy * (clear + half);
            final int slides = i == 0 ? 0 : PANEL_SLIDE_STEPS;
            for (int step = 0; step <= slides; step++) {
                final float back = slides == 0 ? 0.0f : alongExtent * step / slides;
                final float cx = baseX - dx * back;
                final float cy = baseY - dy * back;
                if (cx - out.hitWidth * 0.5f >= 0.0f && cy - out.hitHeight * 0.5f >= 0.0f
                        && cx + out.hitWidth * 0.5f <= viewportW
                        && cy + out.hitHeight * 0.5f <= viewportH) {
                    out.visible = true;
                    out.placement = names[i];
                    out.centreX = cx;
                    out.centreY = cy;
                    return out;
                }
            }
        }
        return out;
    }

    /**
     * The deterministic steps a beside placement tries while sliding back along
     * the shaft: 0, 1/4, 1/2, 3/4 and all of the box's extent along the arrow.
     */
    static final int PANEL_SLIDE_STEPS = 4;

    /**
     * Who owns a touch at {@code (x, y)}: the panel's ONE group proxy, or
     * nobody (the viewport). There is no per-glyph answer to give — the plate
     * takes no touch — so ownership can never depend on how close two shrunken
     * glyphs stand.
     */
    static boolean panelOwnsTouch(PanelLayout layout, float x, float y) {
        return layout.visible && Math.abs(x - layout.centreX) <= layout.hitWidth * 0.5f
                && Math.abs(y - layout.centreY) <= layout.hitHeight * 0.5f;
    }

    /**
     * The shortest distance from the arrow's point to the panel's proxy box, px.
     * At least the corridor plus the clear gap whenever the panel is visible.
     */
    static float panelClearance(PanelLayout layout) {
        final float dx = Math.max(0.0f,
                Math.abs(layout.anchorX - layout.centreX) - layout.hitWidth * 0.5f);
        final float dy = Math.max(0.0f,
                Math.abs(layout.anchorY - layout.centreY) - layout.hitHeight * 0.5f);
        return (float) Math.hypot(dx, dy);
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
