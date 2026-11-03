package com.forgeshape.app;

/**
 * What the pattern editor shows for each kind and what it opens with
 * (`CAD-PATTERN-R1`). Pure decisions over values, so they are pinned on the
 * JVM; the panel only draws them.
 *
 * <p>Nothing here is truth. Native owns the pattern; the seeds it reports
 * ({@link NativeViewport#patternOfferState}, {@link
 * NativeViewport#cadPatternSeeds}) are where a NEW pattern starts, so the first
 * Apply lands the copies beside the original rather than on top of it.
 */
final class PatternPresentation {

    private PatternPresentation() {}

    /** The kind's user-facing name. */
    static int kindName(int kind) {
        switch (kind) {
            case NativeViewport.PATTERN_CIRCULAR:
                return R.string.pattern_circular;
            case NativeViewport.PATTERN_MIRROR:
                return R.string.pattern_mirror;
            default:
                return R.string.pattern_linear;
        }
    }

    /** Linear and Circular walk a direction; a Mirror has no direction to reverse. */
    static boolean reverseShown(int kind) {
        return kind != NativeViewport.PATTERN_MIRROR;
    }

    /** A Mirror makes exactly one copy, so it has no count. */
    static boolean countShown(int kind) {
        return kind != NativeViewport.PATTERN_MIRROR;
    }

    /** Only a Linear pattern has a second direction. */
    static boolean secondDirectionShown(int kind) {
        return kind == NativeViewport.PATTERN_LINEAR;
    }

    /** What the one value field holds. */
    static int valueLabel(int kind) {
        switch (kind) {
            case NativeViewport.PATTERN_CIRCULAR:
                return R.string.pattern_label_angle;
            case NativeViewport.PATTERN_MIRROR:
                return R.string.pattern_label_offset;
            default:
                return R.string.pattern_label_spacing;
        }
    }

    /** A Circular value is degrees; the other two are lengths in the display unit. */
    static boolean valueIsLength(int kind) {
        return kind != NativeViewport.PATTERN_CIRCULAR;
    }

    /** What the axis chips choose: a direction, a rotation axis, or a plane's normal. */
    static int axisLabel(int kind) {
        switch (kind) {
            case NativeViewport.PATTERN_CIRCULAR:
                return R.string.pattern_label_axis_rotate;
            case NativeViewport.PATTERN_MIRROR:
                return R.string.pattern_label_axis_mirror;
            default:
                return R.string.pattern_label_axis_direction;
        }
    }

    /**
     * The instance count a new pattern of {@code kind} along {@code axis}
     * opens with: for Linear, the run native says fits the part.
     */
    static int defaultCount(int kind, int axis, double[] seeds) {
        if (kind == NativeViewport.PATTERN_LINEAR) {
            final int run = (int) Math.abs(seeds[NativeViewport.PATTERN_SEED_RUN_X
                    + clampAxis(axis)]);
            return run >= 2 ? run : 2;
        }
        return defaultCount(kind);
    }

    /** Whether a new Linear run along {@code axis} starts toward -axis, to stay on the part. */
    static boolean defaultReversed(int kind, int axis, double[] seeds) {
        return kind == NativeViewport.PATTERN_LINEAR
                && seeds[NativeViewport.PATTERN_SEED_RUN_X + clampAxis(axis)] < 0.0;
    }

    /** The instance count a new pattern of {@code kind} opens with, without seeds. */
    static int defaultCount(int kind) {
        switch (kind) {
            case NativeViewport.PATTERN_CIRCULAR:
                return 6;
            case NativeViewport.PATTERN_MIRROR:
                return 2;
            default:
                return 3;
        }
    }

    /** The axis a new pattern of {@code kind} opens with, from native's seeds. */
    static int defaultAxis(int kind, double[] seeds) {
        switch (kind) {
            case NativeViewport.PATTERN_CIRCULAR:
                return clampAxis((int) seeds[NativeViewport.PATTERN_SEED_CIRCULAR_AXIS]);
            case NativeViewport.PATTERN_MIRROR:
                return clampAxis((int) seeds[NativeViewport.PATTERN_SEED_MIRROR_AXIS]);
            default:
                return clampAxis((int) seeds[NativeViewport.PATTERN_SEED_LINEAR_AXIS]);
        }
    }

    /**
     * The value a pattern of {@code kind} along {@code axis} opens with: the
     * seeded spacing along that axis, a full turn, or the plane through the
     * part's centre.
     */
    static double defaultValue(int kind, int axis, double[] seeds) {
        switch (kind) {
            case NativeViewport.PATTERN_CIRCULAR:
                return 360.0;
            case NativeViewport.PATTERN_MIRROR:
                return seeds[NativeViewport.PATTERN_SEED_MID_X + clampAxis(axis)];
            default: {
                final double spacing = seeds[NativeViewport.PATTERN_SEED_SPACING_X + clampAxis(axis)];
                return spacing > 0.0 ? spacing : 0.01;
            }
        }
    }

    /** The second direction a Linear grid offers first: the next axis round. */
    static int nextAxis(int axis) {
        return (clampAxis(axis) + 1) % 3;
    }

    static int clampAxis(int axis) {
        return axis < NativeViewport.AXIS_X || axis > NativeViewport.AXIS_Z
                ? NativeViewport.AXIS_X : axis;
    }

    /** The axis's one-letter name. */
    static int axisName(int axis) {
        switch (clampAxis(axis)) {
            case NativeViewport.AXIS_Y:
                return R.string.pattern_axis_y;
            case NativeViewport.AXIS_Z:
                return R.string.pattern_axis_z;
            default:
                return R.string.pattern_axis_x;
        }
    }

    /**
     * Parses a whole instance count; -1 when it is not a whole number. Range is
     * native's to judge, so a count of 1 is reported by native's own refusal.
     */
    static int parseCount(String raw) {
        try {
            return Integer.parseInt(raw.trim());
        } catch (NumberFormatException notACount) {
            return -1;
        }
    }
}
