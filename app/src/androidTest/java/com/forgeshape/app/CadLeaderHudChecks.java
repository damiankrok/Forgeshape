package com.forgeshape.app;

import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

/**
 * Device-side checks of the extrude HUD's technical-drawing leader
 * (`CAD-FOUNDATION-C1`) and its action dock
 * (`CAD-V6-S2-OWNER-CORRECTION-E2E-R1`), shared by every class that asserts
 * where the HUD stands.
 *
 * <p>Each check states a GEOMETRIC property of what is on screen against what
 * native projected — the value reads along its leader and stands on its
 * reading-up side; the dock stands over its projected quad, past the arrow's
 * drawn point on the arrow's own screen line — rather than
 * re-running {@link CadHudPresentation}'s arithmetic and comparing it with
 * itself. Every method returns null when the property holds, or a message that
 * names the numbers when it does not, so a caller can assert or record it.
 */
final class CadLeaderHudChecks {

    /** How far a rendered position may stray from its geometric rule, in dp. */
    static final float TOLERANCE_DP = 4.0f;

    private CadLeaderHudChecks() {
    }

    /**
     * The centre of a placed view in viewport pixels. A rotation about the
     * default (centre) pivot leaves the centre where it is, so the centre is
     * read from the layout box and the translation, never from the rotated
     * corner {@code getLocationInWindow} would report.
     */
    static float[] centreIn(View shown, View viewport) {
        final int[] parent = new int[2];
        final int[] port = new int[2];
        ((View) shown.getParent()).getLocationInWindow(parent);
        viewport.getLocationInWindow(port);
        final float x = parent[0] + shown.getLeft() + shown.getTranslationX()
                + shown.getWidth() * 0.5f - port[0];
        final float y = parent[1] + shown.getTop() + shown.getTranslationY()
                + shown.getHeight() * 0.5f - port[1];
        return new float[]{x, y};
    }

    /** The leader's start and end for one side, or null when it does not project. */
    static float[] leader(double[] tool, boolean second) {
        final int flag = second ? NativeViewport.CAD_EXTRUDE_SECOND_LEADER_ON_SCREEN
                : NativeViewport.CAD_EXTRUDE_LEADER_ON_SCREEN;
        if (tool[flag] == 0.0) {
            return null;
        }
        return new float[]{(float) tool[flag + 1], (float) tool[flag + 2], (float) tool[flag + 3],
                (float) tool[flag + 4]};
    }

    /**
     * Where the geometric rule puts a value's centre, in viewport pixels: the
     * middle of the VISIBLE part of its leader, carried to the reading-up side
     * by half the text's own height and the stated gap. Null when the leader
     * does not project or is wholly off screen. For recorders that compare an
     * expected point with a measured one.
     */
    static float[] expectedValueCentre(double[] tool, TextView value, View viewport,
                                       float density, boolean second) {
        final float[] l = leader(tool, second);
        if (l == null) {
            return null;
        }
        final float[] visible = CadHudPresentation.clipToViewport(l[0], l[1], l[2], l[3],
                viewport.getWidth(), viewport.getHeight());
        if (visible == null) {
            return null;
        }
        final float angle = CadHudPresentation.readingAngleDegrees(l[2] - l[0], l[3] - l[1]);
        final double r = Math.toRadians(angle);
        final float ux = (float) Math.sin(r);
        final float uy = (float) -Math.cos(r);
        final android.graphics.Paint.FontMetrics metrics = value.getPaint().getFontMetrics();
        final float above = (metrics.descent - metrics.ascent) * 0.5f
                + CadHudPresentation.VALUE_GAP_DP * density;
        return new float[]{(visible[0] + visible[2]) * 0.5f + ux * above,
                (visible[1] + visible[3]) * 0.5f + uy * above};
    }

    /**
     * The value stands above its leader: rotated to the leader's upright
     * reading angle, on the reading-up side at a small positive distance, along
     * the line within its visible extent, as an unscaled proxy of >= 48 dp.
     */
    static String valueOnLeader(double[] tool, TextView value, View viewport, float density,
                                boolean second) {
        final float[] l = leader(tool, second);
        if (l == null) {
            return "the leader does not project";
        }
        if (!value.isShown()) {
            return "the value is not shown";
        }
        final float expected = CadHudPresentation.readingAngleDegrees(l[2] - l[0], l[3] - l[1]);
        if (Math.abs(value.getRotation() - expected) > 0.5f) {
            return "rotation " + value.getRotation() + " != upright leader angle " + expected;
        }
        final String proxy = proxy(value, density);
        if (proxy != null) {
            return "value " + proxy;
        }
        final double r = Math.toRadians(expected);
        final float rx = (float) Math.cos(r);
        final float ry = (float) Math.sin(r);
        final float ux = ry;
        final float uy = -rx;
        final float[] c = centreIn(value, viewport);
        final float mx = (l[0] + l[2]) * 0.5f;
        final float my = (l[1] + l[3]) * 0.5f;
        final float above = ((c[0] - mx) * ux + (c[1] - my) * uy) / density;
        final float textDp = value.getTextSize() / density;
        if (!(above > 0.5f) || above > textDp + CadHudPresentation.VALUE_GAP_DP + TOLERANCE_DP) {
            return "value stands " + above + " dp above its leader (text " + textDp + " dp)";
        }
        final float halfLength = (float) Math.hypot(l[2] - l[0], l[3] - l[1]) * 0.5f;
        final float along = Math.abs((c[0] - mx) * rx + (c[1] - my) * ry);
        final boolean clipped = l[0] < 0 || l[1] < 0 || l[2] < 0 || l[3] < 0
                || Math.max(l[0], l[2]) > viewport.getWidth()
                || Math.max(l[1], l[3]) > viewport.getHeight();
        if (!clipped && along > halfLength + TOLERANCE_DP * density) {
            return "value is " + along / density + " dp along a leader of half-length "
                    + halfLength / density + " dp";
        }
        return null;
    }

    /**
     * The action DOCK (`CAD-V6-S2-OWNER-CORRECTION-E2E-R1`): native says it is
     * drawn; its view is shown, never scaled or rotated, and stands exactly
     * over the bounds of its touch shape (the projected quad united with the
     * 48 dp floor square on its centre); every corner of the quad is on the
     * viewport; and its projected centre stands ON the arrow's screen line,
     * beyond the arrow's drawn point -- attached to the geometry, never beside
     * it. Null when all of that holds.
     */
    static String dockAtArrow(double[] tool, View canvas, View viewport, float density) {
        if (tool[NativeViewport.CAD_EXTRUDE_HEAD_ON_SCREEN] == 0.0) {
            return "the arrow's point does not project";
        }
        if (tool[NativeViewport.CAD_EXTRUDE_DOCK_VISIBLE] == 0.0) {
            return "native hides the dock (reason "
                    + (int) tool[NativeViewport.CAD_EXTRUDE_DOCK_HIDDEN] + ")";
        }
        final View dock = canvas.findViewById(R.id.cad_extrude_panel);
        if (!dock.isShown()) {
            return "the dock is not shown";
        }
        if (dock.getScaleX() != 1.0f || dock.getScaleY() != 1.0f || dock.getRotation() != 0.0f) {
            return "the dock's view is scaled or rotated";
        }
        final float floorPx = CadHudPresentation.hitPx(density);
        final CadHud3dPresentation.Dock expected =
                CadHud3dPresentation.fromToolState(tool, floorPx);
        if (!expected.visible) {
            return "the tool state's dock does not read as visible";
        }
        final int[] parent = new int[2];
        final int[] port = new int[2];
        ((View) dock.getParent()).getLocationInWindow(parent);
        viewport.getLocationInWindow(port);
        final float left = parent[0] + dock.getLeft() + dock.getTranslationX() - port[0];
        final float top = parent[1] + dock.getTop() + dock.getTranslationY() - port[1];
        if (Math.abs(left - expected.left) > 2.0f || Math.abs(top - expected.top) > 2.0f
                || dock.getWidth() + 2 < expected.right - expected.left
                || dock.getHeight() + 2 < expected.bottom - expected.top) {
            return "the dock's box " + left + "," + top + " " + dock.getWidth() + "x"
                    + dock.getHeight() + " is not its touch shape's bounds " + expected.left
                    + "," + expected.top + " .. " + expected.right + "," + expected.bottom;
        }
        if (dock.getWidth() < floorPx - 1 || dock.getHeight() < floorPx - 1) {
            return "the dock's box is under the 48 dp floor";
        }
        for (int i = 0; i < 4; i++) {
            final float x = expected.quad[2 * i];
            final float y = expected.quad[2 * i + 1];
            if (x < -1 || y < -1 || x > viewport.getWidth() + 1 || y > viewport.getHeight() + 1) {
                return "a dock corner is off the viewport: " + x + "," + y;
            }
        }
        // Attached: the centre stands ON the arrow's screen line, past its point.
        final float hx = (float) tool[NativeViewport.CAD_EXTRUDE_HEAD_X];
        final float hy = (float) tool[NativeViewport.CAD_EXTRUDE_HEAD_Y];
        float ax = hx - (float) tool[NativeViewport.CAD_EXTRUDE_LABEL_X];
        float ay = hy - (float) tool[NativeViewport.CAD_EXTRUDE_LABEL_Y];
        final float axisLength = (float) Math.hypot(ax, ay);
        if (!(axisLength > 1.0f)) {
            return "the arrow has no screen direction";
        }
        ax /= axisLength;
        ay /= axisLength;
        final float along = (expected.centreX - hx) * ax + (expected.centreY - hy) * ay;
        final float across = Math.abs(-(expected.centreX - hx) * ay + (expected.centreY - hy) * ax);
        if (across > TOLERANCE_DP * density) {
            return "the dock stands " + across / density + " dp off the arrow's line";
        }
        if (!(along > 0.0f)) {
            return "the dock is not past the arrow's point (" + along / density + " dp)";
        }
        if (!CadHud3dPresentation.claims(expected, expected.centreX, expected.centreY, floorPx)) {
            return "the dock does not claim its own centre";
        }
        return null;
    }

    /** The dock's distance from the arrow's point to its centre, in dp; for records. */
    static float dockReachDp(double[] tool, float density) {
        return (float) Math.hypot(
                tool[NativeViewport.CAD_EXTRUDE_HEAD_X] - tool[NativeViewport.CAD_EXTRUDE_DOCK_CENTRE_X],
                tool[NativeViewport.CAD_EXTRUDE_HEAD_Y] - tool[NativeViewport.CAD_EXTRUDE_DOCK_CENTRE_Y])
                / density;
    }

    /** An unscaled touch proxy of at least 48 dp each way. */
    static String proxy(View control, float density) {
        final int floor = Math.round(48f * density) - 1;
        if (control.getWidth() < floor || control.getHeight() < floor) {
            return "proxy " + control.getWidth() + "x" + control.getHeight() + " px under 48 dp";
        }
        if (control.getScaleX() != 1.0f || control.getScaleY() != 1.0f) {
            return "the proxy is scaled";
        }
        return null;
    }

    static ImageView firstImage(View view) {
        if (view instanceof ImageView) {
            return (ImageView) view;
        }
        if (view instanceof ViewGroup) {
            final ViewGroup group = (ViewGroup) view;
            for (int i = 0; i < group.getChildCount(); ++i) {
                final ImageView found = firstImage(group.getChildAt(i));
                if (found != null) {
                    return found;
                }
            }
        }
        return null;
    }
}
