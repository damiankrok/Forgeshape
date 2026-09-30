package com.forgeshape.app;

import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

/**
 * Device-side checks of the extrude HUD's technical-drawing leader
 * (`CAD-FOUNDATION-C1`), shared by every class that used to assert the old
 * "value centred on the shaft" rule.
 *
 * <p>Each check states a GEOMETRIC property of what is on screen against what
 * native projected — the value reads along its leader and stands on its
 * reading-up side, a glyph stands on the leader's line past an end, every
 * control is an unscaled, invisible touch proxy of at least 48 dp — rather than
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
     * An attached glyph: an invisible, unscaled proxy of >= 48 dp whose drawn
     * glyph is the camera-attached size, standing ON the leader's line past
     * the end named ({@code pastTip} true: past the tip end).
     */
    static String glyphOnLeader(double[] tool, View control, View viewport, float density,
                                boolean pastTip) {
        final float[] l = leader(tool, false);
        if (l == null) {
            return "the leader does not project";
        }
        final String proxy = proxy(control, density);
        if (proxy != null) {
            return proxy;
        }
        if (control.getBackground() != null) {
            return "the touch proxy paints a background";
        }
        final ImageView glyph = firstImage(control);
        if (glyph == null) {
            return "no glyph";
        }
        final int expected = CadHudPresentation.glyphPx(tool[NativeViewport.CAD_EXTRUDE_SCALE],
                density);
        if (Math.abs(glyph.getWidth() - expected) > 1 || Math.abs(glyph.getHeight() - expected) > 1) {
            return "glyph " + glyph.getWidth() + "x" + glyph.getHeight() + " px, expected "
                    + expected;
        }
        final float[] c = centreIn(control, viewport);
        float dx = l[2] - l[0];
        float dy = l[3] - l[1];
        final float length = (float) Math.hypot(dx, dy);
        if (length < 1.0f) {
            return null;  // seen end-on: nothing to be on
        }
        dx /= length;
        dy /= length;
        final float off = Math.abs((c[0] - l[0]) * dy - (c[1] - l[1]) * dx) / density;
        if (off > TOLERANCE_DP) {
            return "glyph stands " + off + " dp off its leader's line";
        }
        final float along = (c[0] - l[0]) * dx + (c[1] - l[1]) * dy;
        if (pastTip ? along <= length : along >= 0.0f) {
            return "glyph is not past the " + (pastTip ? "tip" : "base") + " end (along=" + along
                    + ", length=" + length + ")";
        }
        return null;
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
