package com.forgeshape.app;

/**
 * What the hole editor shows for each kind and what a NEW hole opens with
 * (`CAD-HOLE-R1`). Pure decisions over values, so they are pinned on the JVM;
 * the panel only draws them.
 *
 * <p>Nothing here is truth. Native owns the hole and judges every value; the
 * seeds below only make the first Apply land a hole a person would drill — a
 * counterbore wider than the bore and shallower than the hole, a 90° cone for
 * a flat screw head — rather than one native would refuse.
 */
final class HolePresentation {

    private HolePresentation() {}

    /** The included angle a new countersink opens with: the common metric one. */
    static final double DEFAULT_SINK_ANGLE = 90.0;

    /** The kind's user-facing name. */
    static int kindName(int kind) {
        switch (kind) {
            case NativeViewport.HOLE_COUNTERBORE:
                return R.string.hole_counterbore;
            case NativeViewport.HOLE_COUNTERSINK:
                return R.string.hole_countersink;
            default:
                return R.string.hole_simple;
        }
    }

    /** What the kind chip says to a screen reader. */
    static int kindDescription(int kind) {
        switch (kind) {
            case NativeViewport.HOLE_COUNTERBORE:
                return R.string.hole_kind_counterbore_description;
            case NativeViewport.HOLE_COUNTERSINK:
                return R.string.hole_kind_countersink_description;
            default:
                return R.string.hole_kind_simple_description;
        }
    }

    /** Only a Counterbore has a bore row. */
    static boolean boreShown(int kind) {
        return kind == NativeViewport.HOLE_COUNTERBORE;
    }

    /** Only a Countersink has a cone row. */
    static boolean sinkShown(int kind) {
        return kind == NativeViewport.HOLE_COUNTERSINK;
    }

    /** The 1/2/5 x 10^k value at or above {@code v}; 0 for none. */
    static double niceAtLeast(double v) {
        if (!(v > 0.0) || Double.isInfinite(v)) {
            return 0.0;
        }
        final double decade = Math.pow(10.0, Math.floor(Math.log10(v)));
        for (double m : new double[] {1.0, 2.0, 5.0, 10.0}) {
            if (m * decade >= v * (1.0 - 1e-9)) {
                return m * decade;
            }
        }
        return 10.0 * decade;
    }

    /** A new blind hole's depth: twice its diameter, rounded up. */
    static double defaultDepth(double diameter) {
        return niceAtLeast(2.0 * diameter);
    }

    /** A new counterbore's diameter: clearly wider than the hole. */
    static double defaultBoreDiameter(double diameter) {
        return niceAtLeast(1.6 * diameter);
    }

    /** A new counterbore's depth: half the diameter, rounded up. */
    static double defaultBoreDepth(double diameter) {
        return niceAtLeast(0.5 * diameter);
    }

    /** A new countersink's diameter at the face: twice the hole's. */
    static double defaultSinkDiameter(double diameter) {
        return niceAtLeast(2.0 * diameter);
    }
}
