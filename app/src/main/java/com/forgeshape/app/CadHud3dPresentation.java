package com.forgeshape.app;

/**
 * The extrude action DOCK's presentation rules, as plain values
 * (`CAD-V6-S2-OWNER-CORRECTION-E2E-R1`, HUD3D).
 *
 * <p><b>What the dock is.</b> One compact badge stating the operation, drawn
 * INTO a quadrilateral native projected from a world rectangle standing on
 * the extrusion axis just past the arrow's drawn point
 * ({@code cadExtrudeDockFor}). Native owns the frame, the size, the fade and
 * the decision to hide it whole; this class only reads the four projected
 * corners back out of the tool state, maps a flat badge onto them with a
 * perspective homography, and says which touches the dock claims. There is no
 * screen-edge rule here: no slide, no clamp, no candidate side, no rotation
 * policy. A dock whose corners do not all stand on the viewport is simply not
 * drawn, because native says so.
 *
 * <p><b>The touch claim.</b> The projected quad, united with a square of the
 * 48 dp interactive floor centred on the dock's projected centre — and nothing
 * else. The dock's Android box is the axis-aligned bounds of that shape, but a
 * Down anywhere else in the box is declined, so the cells around a small or
 * skewed badge stay tappable.
 *
 * <p>Holds no Android type and reads no state, so the JVM pins every rule.
 */
final class CadHud3dPresentation {

    private CadHud3dPresentation() {
    }

    /** One frame's dock, in viewport pixels. */
    static final class Dock {
        /** False: the whole dock is absent this frame. */
        boolean visible;
        /** The near-axis fade, (0, 1] when visible. */
        float alpha;
        /** Top-left, top-right, bottom-right, bottom-left (x, y pairs): reading order. */
        final float[] quad = new float[8];
        float centreX;
        float centreY;
        /** The touch shape's bounds: the quad united with the floor square. */
        float left;
        float top;
        float right;
        float bottom;
        /** Why native hid it (`CadExtrudeDockHidden`), for evidence. */
        int hiddenReason;
        /** |sin| of the view against the axis at the arrow point, for evidence. */
        float axisSine;
    }

    /**
     * Reads the dock out of a {@code CAD_EXTRUDE_*} tool state and lays out its
     * touch bounds for an interactive floor of {@code floorPx}.
     */
    static Dock fromToolState(double[] tool, float floorPx) {
        final Dock dock = new Dock();
        if (tool == null || tool.length < NativeViewport.CAD_EXTRUDE_SIZE) {
            return dock;
        }
        dock.hiddenReason = (int) tool[NativeViewport.CAD_EXTRUDE_DOCK_HIDDEN];
        dock.axisSine = (float) tool[NativeViewport.CAD_EXTRUDE_DOCK_AXIS_SINE];
        if (tool[NativeViewport.CAD_EXTRUDE_ACTIVE] == 0.0
                || tool[NativeViewport.CAD_EXTRUDE_DOCK_VISIBLE] == 0.0) {
            return dock;
        }
        for (int i = 0; i < 8; i++) {
            final float value = (float) tool[NativeViewport.CAD_EXTRUDE_DOCK_TL_X + i];
            if (!isFinite(value)) {
                return dock;
            }
            dock.quad[i] = value;
        }
        dock.centreX = (float) tool[NativeViewport.CAD_EXTRUDE_DOCK_CENTRE_X];
        dock.centreY = (float) tool[NativeViewport.CAD_EXTRUDE_DOCK_CENTRE_Y];
        dock.alpha = (float) tool[NativeViewport.CAD_EXTRUDE_DOCK_ALPHA];
        if (!isFinite(dock.centreX) || !isFinite(dock.centreY) || !(dock.alpha > 0.0f)) {
            return dock;
        }
        dock.alpha = Math.min(1.0f, dock.alpha);
        final float[] bounds = touchBounds(dock.quad, dock.centreX, dock.centreY, floorPx);
        dock.left = bounds[0];
        dock.top = bounds[1];
        dock.right = bounds[2];
        dock.bottom = bounds[3];
        dock.visible = true;
        return dock;
    }

    /**
     * The axis-aligned bounds {@code {left, top, right, bottom}} of the quad
     * united with a {@code floorPx} square centred on (cx, cy).
     */
    static float[] touchBounds(float[] quad, float cx, float cy, float floorPx) {
        final float half = floorPx * 0.5f;
        float left = cx - half;
        float top = cy - half;
        float right = cx + half;
        float bottom = cy + half;
        for (int i = 0; i < 4; i++) {
            left = Math.min(left, quad[2 * i]);
            right = Math.max(right, quad[2 * i]);
            top = Math.min(top, quad[2 * i + 1]);
            bottom = Math.max(bottom, quad[2 * i + 1]);
        }
        return new float[]{left, top, right, bottom};
    }

    /**
     * Whether the dock claims a touch at (x, y), in viewport pixels: inside the
     * projected quad or inside the floor square on its centre. Never anywhere
     * else in its box.
     */
    static boolean claims(Dock dock, float x, float y, float floorPx) {
        if (dock == null || !dock.visible) {
            return false;
        }
        final float half = floorPx * 0.5f;
        if (Math.abs(x - dock.centreX) <= half && Math.abs(y - dock.centreY) <= half) {
            return true;
        }
        return insideQuad(dock.quad, x, y);
    }

    /**
     * Whether (x, y) is inside a convex quad given in either winding; a point
     * on an edge is inside.
     */
    static boolean insideQuad(float[] quad, float x, float y) {
        boolean anyPositive = false;
        boolean anyNegative = false;
        for (int i = 0; i < 4; i++) {
            final int j = (i + 1) % 4;
            final float ex = quad[2 * j] - quad[2 * i];
            final float ey = quad[2 * j + 1] - quad[2 * i + 1];
            final float cross = ex * (y - quad[2 * i + 1]) - ey * (x - quad[2 * i]);
            if (cross > 0.0f) anyPositive = true;
            if (cross < 0.0f) anyNegative = true;
            if (anyPositive && anyNegative) {
                return false;
            }
        }
        return true;
    }

    /**
     * The perspective homography carrying the source box {@code [0, w] x [0, h]}
     * onto {@code quad} (top-left, top-right, bottom-right, bottom-left), as the
     * nine values {@code android.graphics.Matrix#setValues} takes, row-major:
     * {@code x' = (m0 x + m1 y + m2) / (m6 x + m7 y + m8)}. Null for a
     * degenerate quad. The same answer {@code Matrix.setPolyToPoly(..., 4)}
     * gives, stated here so the JVM can hold it.
     */
    static float[] homography(float w, float h, float[] quad) {
        if (!(w > 0.0f) || !(h > 0.0f) || quad == null || quad.length < 8) {
            return null;
        }
        final double x0 = quad[0], y0 = quad[1];
        final double x1 = quad[2], y1 = quad[3];
        final double x2 = quad[4], y2 = quad[5];
        final double x3 = quad[6], y3 = quad[7];
        // A quad with no area maps the badge to a line or a point: no matrix.
        final double area = 0.5 * ((x0 * y1 - x1 * y0) + (x1 * y2 - x2 * y1)
                + (x2 * y3 - x3 * y2) + (x3 * y0 - x0 * y3));
        if (!(Math.abs(area) >= 1.0)) {
            return null;
        }
        final double sx = x0 - x1 + x2 - x3;
        final double sy = y0 - y1 + y2 - y3;
        double g = 0.0;
        double k = 0.0;
        if (sx != 0.0 || sy != 0.0) {
            final double dx1 = x1 - x2;
            final double dx2 = x3 - x2;
            final double dy1 = y1 - y2;
            final double dy2 = y3 - y2;
            final double det = dx1 * dy2 - dx2 * dy1;
            if (Math.abs(det) < 1e-9) {
                return null;
            }
            g = (sx * dy2 - dx2 * sy) / det;
            k = (dx1 * sy - sx * dy1) / det;
        }
        // Unit square -> quad.
        final double a = x1 - x0 + g * x1;
        final double b = x3 - x0 + k * x3;
        final double d = y1 - y0 + g * y1;
        final double e = y3 - y0 + k * y3;
        final double[] m = {a / w, b / h, x0, d / w, e / h, y0, g / w, k / h, 1.0};
        final float[] out = new float[9];
        for (int i = 0; i < 9; i++) {
            if (!Double.isFinite(m[i])) {
                return null;
            }
            out[i] = (float) m[i];
        }
        return out;
    }

    /** Applies a {@link #homography} to (x, y); {x', y'}. */
    static float[] map(float[] m, float x, float y) {
        final float w = m[6] * x + m[7] * y + m[8];
        return new float[]{(m[0] * x + m[1] * y + m[2]) / w, (m[3] * x + m[4] * y + m[5]) / w};
    }

    /**
     * The side of the square source box a badge is drawn into, px: the quad's
     * longest edge, so the homography stays near unit scale and a vector glyph
     * is rasterised at about the size it is seen at.
     */
    static float sourceSide(float[] quad) {
        float longest = 1.0f;
        for (int i = 0; i < 4; i++) {
            final int j = (i + 1) % 4;
            longest = Math.max(longest, (float) Math.hypot(quad[2 * j] - quad[2 * i],
                    quad[2 * j + 1] - quad[2 * i + 1]));
        }
        return longest;
    }

    private static boolean isFinite(float value) {
        return !Float.isNaN(value) && !Float.isInfinite(value);
    }
}
