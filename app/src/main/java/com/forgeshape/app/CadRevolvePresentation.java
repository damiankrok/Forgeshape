package com.forgeshape.app;

import java.math.BigDecimal;
import java.util.Locale;

/**
 * What the Revolve chrome shows, as pure functions over the one
 * {@link NativeViewport#cadRevolveToolState} read (`CAD-V6-REVOLVE-NEWBODY-E2E-R1`).
 *
 * <p>No Android type, no state, no semantics: the axis, the angle, the
 * direction and whether the candidate would be refused are all native's, and
 * this class only decides which of the chrome's controls a given state draws
 * and how a number in degrees is written and read. Kept free of Android so the
 * JVM can pin it.
 */
final class CadRevolvePresentation {

    private CadRevolvePresentation() {}

    /** The precision an angle is displayed at: three decimals, trailing zeros dropped. */
    static final int ANGLE_DISPLAY_DECIMALS = 3;

    /**
     * The handle's touch proxy radius, in dp — the native grab radius
     * ({@code kRevolveHandleGrabUnits}), mirrored so the JVM can pin what a
     * nearby tap is and is not claimed by. Native decides; this documents.
     */
    static final float HANDLE_GRAB_DP = 28.0f;

    /** The 48 dp interactive floor every revolve control's hit area meets. */
    static final float MIN_TARGET_DP = 48.0f;

    /**
     * An angle as the user reads it: {@code 360}, {@code 180}, {@code 37.5},
     * never {@code 37.50000000000001}. The value is native's and is never
     * rounded where it is stored; this is display.
     */
    static String formatDegrees(double degrees) {
        if (Double.isNaN(degrees) || Double.isInfinite(degrees)) {
            return "—";
        }
        BigDecimal value = new BigDecimal(degrees).setScale(ANGLE_DISPLAY_DECIMALS,
                java.math.RoundingMode.HALF_EVEN).stripTrailingZeros();
        if (value.scale() < 0) {
            value = value.setScale(0);
        }
        return value.toPlainString();
    }

    /**
     * A typed angle, in degrees: digits with one decimal point or comma, an
     * optional sign and an optional trailing degree sign. {@code NaN} when the
     * text is not a number — the caller reports it by name. Range is native's
     * to judge: 0, negative and above 360 parse and are then REFUSED below JNI,
     * never clamped here.
     */
    static double parseDegrees(String raw) {
        if (raw == null) {
            return Double.NaN;
        }
        String text = raw.trim();
        if (text.endsWith("°")) {
            text = text.substring(0, text.length() - 1).trim();
        }
        text = text.replace(',', '.');
        if (text.isEmpty()) {
            return Double.NaN;
        }
        try {
            return new BigDecimal(text).doubleValue();
        } catch (NumberFormatException notANumber) {
            return Double.NaN;
        }
    }

    /** Whether the state describes a revolve in progress. */
    static boolean active(double[] state) {
        return state[NativeViewport.REVOLVE_ACTIVE] != 0.0;
    }

    /** Whether a tap picks the axis now. */
    static boolean axisPicking(double[] state) {
        return active(state) && state[NativeViewport.REVOLVE_AXIS_PICKING] != 0.0;
    }

    /**
     * Whether the precision surface offers "Revolve…": the session can revolve,
     * is not already revolving, and at least one area is chosen — an entry that
     * could not succeed is not drawn.
     */
    static boolean entryShown(double[] state) {
        return !active(state) && state[NativeViewport.REVOLVE_AVAILABLE] != 0.0
                && state[NativeViewport.REVOLVE_SELECTED_AREAS] > 0.0;
    }

    /** Whether the commit (toolbar Revolve) is drawn: a candidate a commit may make. */
    static boolean commitShown(double[] state) {
        return active(state) && !axisPicking(state)
                && (int) state[NativeViewport.REVOLVE_CANDIDATE_STATUS] == NativeViewport.CAD_OK;
    }

    /**
     * Whether "Extrude instead" is drawn: a new body may change its mind; an
     * edit of a revolved body keeps the body's own kind.
     */
    static boolean extrudeInsteadShown(double[] state) {
        return active(state) && state[NativeViewport.REVOLVE_EDITING_REVOLVE_BODY] == 0.0;
    }

    /** Whether the angle label stands on the canvas: active, an axis held, a projected anchor. */
    static boolean labelShown(double[] state) {
        return active(state) && !axisPicking(state)
                && state[NativeViewport.REVOLVE_LABEL_VISIBLE] != 0.0;
    }

    /**
     * Whether a touch at (x, y) lands in the ring handle's proxy around its
     * projected point (hx, hy). Mirrors native's grab test: a touch outside it
     * is never the handle's, so a tap on the profile beside the ring still
     * reaches the area under it.
     */
    static boolean handleProxyContains(float hx, float hy, float x, float y, float density) {
        final float radius = HANDLE_GRAB_DP * density;
        final float dx = x - hx;
        final float dy = y - hy;
        return dx * dx + dy * dy <= radius * radius;
    }

    /**
     * How many straight segments native draws the ring arc of {@code degrees}
     * with: {@code max(8, ceil(64 * degrees / 360))}. Mirrored like a shader
     * constant so the JVM can pin that the arc stays a continuous, bounded
     * polyline at every angle.
     */
    static int ringSegments(double degrees) {
        return Math.max(8, (int) Math.ceil(64.0 * degrees / 360.0));
    }

    /** The arc's unit-circle sample k of {@link #ringSegments}, as {cos, sin}. */
    static double[] ringSample(double degrees, int k) {
        final int steps = ringSegments(degrees);
        final double theta = Math.toRadians(degrees) * k / steps;
        return new double[]{Math.cos(theta), Math.sin(theta)};
    }

    /** "360°", "37.5°" — the label text, locale-independent digits. */
    static String label(double degrees) {
        return String.format(Locale.ROOT, "%s°", formatDegrees(degrees));
    }
}
