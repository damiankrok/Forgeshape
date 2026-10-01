package com.forgeshape.app;

import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

/**
 * Device-side checks of the extrude HUD's technical-drawing leader
 * (`CAD-FOUNDATION-C1`) and its one action panel (`CAD-FOUNDATION-C2`), shared
 * by every class that asserts where the HUD stands.
 *
 * <p>Each check states a GEOMETRIC property of what is on screen against what
 * native projected — the value reads along its leader and stands on its
 * reading-up side; the action panel is ONE plate scaled as a unit, standing
 * just past the arrow's drawn point and clear of its grab corridor, with ONE
 * unscaled touch proxy of at least 48 dp covering it — rather than
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
     * The action panel (`CAD-FOUNDATION-C2`): the plate and its one proxy share
     * a centre; the plate is scaled as ONE unit by the camera-attached visual
     * scale and holds {@code icons} shown glyphs at their reference size; the
     * proxy is unscaled, at least 48 dp each way and covers the plate; and the
     * proxy stands clear of the arrow's grab corridor around native's projected
     * arrow point. Null when all of that holds.
     */
    static String panelAtArrow(double[] tool, View canvas, View viewport, float density,
                               int icons) {
        if (tool[NativeViewport.CAD_EXTRUDE_HEAD_ON_SCREEN] == 0.0) {
            return "the arrow's point does not project";
        }
        final View plate = canvas.findViewById(R.id.cad_extrude_panel_plate);
        final View proxy = canvas.findViewById(R.id.cad_extrude_panel);
        if (!plate.isShown() || !proxy.isShown()) {
            return "the panel is not shown (plate " + plate.isShown() + ", proxy "
                    + proxy.isShown() + ")";
        }
        final String floor = proxy(proxy, density);
        if (floor != null) {
            return "panel " + floor;
        }
        if (proxy.getBackground() != null) {
            return "the touch proxy paints a background";
        }
        if (plate.isClickable()) {
            return "the scaled plate takes touches";
        }
        final float expected = CadHudPresentation.visualScale(
                tool[NativeViewport.CAD_EXTRUDE_SCALE]);
        if (Math.abs(plate.getScaleX() - expected) > 1e-4f
                || plate.getScaleX() != plate.getScaleY()) {
            return "plate scale " + plate.getScaleX() + "x" + plate.getScaleY() + ", expected "
                    + expected + " both ways";
        }
        int shown = 0;
        final int reference = CadHudPresentation.glyphPx(1.0, density);
        final ViewGroup group = (ViewGroup) plate;
        for (int i = 0; i < group.getChildCount(); i++) {
            final View glyph = group.getChildAt(i);
            if (glyph.getVisibility() != View.VISIBLE) {
                continue;
            }
            shown++;
            if (Math.abs(glyph.getWidth() - reference) > 1) {
                return "a plate glyph is " + glyph.getWidth() + " px, not the reference "
                        + reference + ": it was resized alone";
            }
        }
        if (shown != icons) {
            return shown + " glyphs on the plate, expected " + icons;
        }
        // The plate's visual centre: it is scaled and turned about its OWN
        // centre (`CAD-V6-S2-CORRECTION-FILL-HUD-R1`), so that centre is the
        // layout box's, wherever the translation put it.
        final int[] parent = new int[2];
        final int[] port = new int[2];
        ((View) plate.getParent()).getLocationInWindow(parent);
        viewport.getLocationInWindow(port);
        if (Math.abs(plate.getPivotX() - plate.getWidth() * 0.5f) > 1.0f
                || Math.abs(plate.getPivotY() - plate.getHeight() * 0.5f) > 1.0f) {
            return "the plate does not turn and scale about its own centre";
        }
        final float plateCX = parent[0] + plate.getLeft() + plate.getTranslationX()
                + plate.getWidth() * 0.5f - port[0];
        final float plateCY = parent[1] + plate.getTop() + plate.getTranslationY()
                + plate.getHeight() * 0.5f - port[1];
        final float[] turned = CadHudPresentation.rotatedBounds(
                plate.getWidth() * plate.getScaleX(), plate.getHeight() * plate.getScaleY(),
                plate.getRotation());
        final float[] c = centreIn(proxy, viewport);
        if (Math.abs(plateCX - c[0]) > TOLERANCE_DP * density
                || Math.abs(plateCY - c[1]) > TOLERANCE_DP * density) {
            return "the plate is not centred on its proxy";
        }
        if (proxy.getWidth() + 1 < turned[0] || proxy.getHeight() + 1 < turned[1]) {
            return "the proxy does not cover the turned plate";
        }
        if (plateCX - turned[0] * 0.5f < -1 || plateCY - turned[1] * 0.5f < -1
                || plateCX + turned[0] * 0.5f > viewport.getWidth() + 1
                || plateCY + turned[1] * 0.5f > viewport.getHeight() + 1) {
            return "the plate is not wholly on screen";
        }
        if (Math.abs(plate.getRotation()) > CadHudPresentation.PANEL_ROTATION_MAX_DEGREES + 0.01f) {
            return "the plate turns " + plate.getRotation() + " degrees, past the cap";
        }
        // Attached: the centre stands ON the arrow's screen line, between the
        // point and the attached offset past it -- never beside the shaft.
        final float hx = (float) tool[NativeViewport.CAD_EXTRUDE_HEAD_X];
        final float hy = (float) tool[NativeViewport.CAD_EXTRUDE_HEAD_Y];
        float ax = hx - (float) tool[NativeViewport.CAD_EXTRUDE_LABEL_X];
        float ay = hy - (float) tool[NativeViewport.CAD_EXTRUDE_LABEL_Y];
        final float axisLength = (float) Math.hypot(ax, ay);
        if (axisLength > 1.0f) {
            ax /= axisLength;
            ay /= axisLength;
        } else {
            ax = 1.0f;
            ay = 0.0f;
        }
        final float along = (c[0] - hx) * ax + (c[1] - hy) * ay;
        final float across = Math.abs(-(c[0] - hx) * ay + (c[1] - hy) * ax);
        if (across > TOLERANCE_DP * density) {
            return "the panel stands " + across / density + " dp off the arrow's line: beside "
                    + "the shaft, not attached at its point";
        }
        final float attached = (CadHudPresentation.ARROW_CORRIDOR_DP
                + CadHudPresentation.PANEL_CLEAR_DP) * density
                + 0.5f * (proxy.getWidth() * Math.abs(ax) + proxy.getHeight() * Math.abs(ay));
        // From the attached offset past the point back to the shaft's base,
        // twice the middle-to-point vector behind the point.
        final float base = axisLength > 1.0f ? 2.0f * axisLength : 0.0f;
        if (along < -base - TOLERANCE_DP * density || along > attached + TOLERANCE_DP * density) {
            return "the panel stands " + along / density + " dp along the arrow from its point, "
                    + "outside [" + (-base / density) + ", " + attached / density + "]";
        }
        // Clear of the corridor unless an edge made it slide back.
        final float dx = Math.max(0.0f, Math.abs(hx - c[0]) - proxy.getWidth() * 0.5f);
        final float dy = Math.max(0.0f, Math.abs(hy - c[1]) - proxy.getHeight() * 0.5f);
        final float clearance = (float) Math.hypot(dx, dy) / density;
        final boolean atEdge = c[0] - proxy.getWidth() * 0.5f <= 2.0f
                || c[1] - proxy.getHeight() * 0.5f <= 2.0f
                || c[0] + proxy.getWidth() * 0.5f >= viewport.getWidth() - 2.0f
                || c[1] + proxy.getHeight() * 0.5f >= viewport.getHeight() - 2.0f;
        if (!atEdge && clearance < CadHudPresentation.ARROW_CORRIDOR_DP - TOLERANCE_DP) {
            return "the proxy stands " + clearance + " dp from the arrow's point, inside its "
                    + "grab corridor, with no edge to explain it";
        }
        return null;
    }

    /** The panel's distance from the arrow's point to its centre, in dp; for records. */
    static float panelReachDp(double[] tool, View canvas, View viewport, float density) {
        final float[] c = centreIn(canvas.findViewById(R.id.cad_extrude_panel), viewport);
        return (float) Math.hypot(tool[NativeViewport.CAD_EXTRUDE_HEAD_X] - c[0],
                tool[NativeViewport.CAD_EXTRUDE_HEAD_Y] - c[1]) / density;
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
