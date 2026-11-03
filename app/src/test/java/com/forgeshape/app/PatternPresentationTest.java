package com.forgeshape.app;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

/**
 * The pattern editor's presentation rules, on the JVM (`CAD-PATTERN-R1`):
 * which rows each kind draws, what each value field holds, and what a new
 * pattern opens with — a Linear run that fits the part, a full turn, a mirror
 * plane through the part's centre. A device proves the panel obeys it; this
 * proves the rule itself.
 */
public final class PatternPresentationTest {

    private static double[] seeds() {
        final double[] seeds = new double[NativeViewport.PATTERN_SEEDS_SIZE];
        seeds[NativeViewport.PATTERN_SEED_LINEAR_AXIS] = NativeViewport.AXIS_Y;
        seeds[NativeViewport.PATTERN_SEED_SPACING_X] = 0.5;
        seeds[NativeViewport.PATTERN_SEED_SPACING_X + 1] = 0.25;
        seeds[NativeViewport.PATTERN_SEED_SPACING_X + 2] = 0.0;
        seeds[NativeViewport.PATTERN_SEED_CIRCULAR_AXIS] = NativeViewport.AXIS_Z;
        seeds[NativeViewport.PATTERN_SEED_MID_X] = 1.5;
        seeds[NativeViewport.PATTERN_SEED_MID_X + 1] = -2.0;
        seeds[NativeViewport.PATTERN_SEED_MIRROR_AXIS] = NativeViewport.AXIS_X;
        seeds[NativeViewport.PATTERN_SEED_RUN_X] = 3;
        seeds[NativeViewport.PATTERN_SEED_RUN_X + 1] = -2;
        return seeds;
    }

    @Test
    public void eachKindDrawsOnlyTheRowsItUses() {
        assertTrue(PatternPresentation.countShown(NativeViewport.PATTERN_LINEAR));
        assertTrue(PatternPresentation.countShown(NativeViewport.PATTERN_CIRCULAR));
        assertFalse("a mirror makes exactly one copy",
                PatternPresentation.countShown(NativeViewport.PATTERN_MIRROR));
        assertFalse("a mirror has no direction to reverse",
                PatternPresentation.reverseShown(NativeViewport.PATTERN_MIRROR));
        assertTrue(PatternPresentation.secondDirectionShown(NativeViewport.PATTERN_LINEAR));
        assertFalse(PatternPresentation.secondDirectionShown(NativeViewport.PATTERN_CIRCULAR));
        assertFalse(PatternPresentation.secondDirectionShown(NativeViewport.PATTERN_MIRROR));
    }

    @Test
    public void theValueFieldIsALengthExceptForTheAngle() {
        assertTrue(PatternPresentation.valueIsLength(NativeViewport.PATTERN_LINEAR));
        assertFalse(PatternPresentation.valueIsLength(NativeViewport.PATTERN_CIRCULAR));
        assertTrue(PatternPresentation.valueIsLength(NativeViewport.PATTERN_MIRROR));
        assertEquals(R.string.pattern_label_spacing,
                PatternPresentation.valueLabel(NativeViewport.PATTERN_LINEAR));
        assertEquals(R.string.pattern_label_angle,
                PatternPresentation.valueLabel(NativeViewport.PATTERN_CIRCULAR));
        assertEquals(R.string.pattern_label_offset,
                PatternPresentation.valueLabel(NativeViewport.PATTERN_MIRROR));
    }

    @Test
    public void aNewPatternOpensOnNativesSeeds() {
        final double[] seeds = seeds();
        assertEquals(NativeViewport.AXIS_Y,
                PatternPresentation.defaultAxis(NativeViewport.PATTERN_LINEAR, seeds));
        assertEquals(NativeViewport.AXIS_Z,
                PatternPresentation.defaultAxis(NativeViewport.PATTERN_CIRCULAR, seeds));
        assertEquals(NativeViewport.AXIS_X,
                PatternPresentation.defaultAxis(NativeViewport.PATTERN_MIRROR, seeds));
        assertEquals(0.25, PatternPresentation.defaultValue(NativeViewport.PATTERN_LINEAR,
                NativeViewport.AXIS_Y, seeds), 0.0);
        assertEquals("an axis with no seeded spacing still opens on a positive one", 0.01,
                PatternPresentation.defaultValue(NativeViewport.PATTERN_LINEAR,
                        NativeViewport.AXIS_Z, seeds), 0.0);
        assertEquals(360.0, PatternPresentation.defaultValue(NativeViewport.PATTERN_CIRCULAR,
                NativeViewport.AXIS_Z, seeds), 0.0);
        assertEquals("a mirror plane through the part's centre", -2.0,
                PatternPresentation.defaultValue(NativeViewport.PATTERN_MIRROR,
                        NativeViewport.AXIS_Y, seeds), 0.0);
    }

    @Test
    public void aLinearRunOpensOnTheCopiesThatFitAndTheirDirection() {
        final double[] seeds = seeds();
        assertEquals(3, PatternPresentation.defaultCount(NativeViewport.PATTERN_LINEAR,
                NativeViewport.AXIS_X, seeds));
        assertFalse(PatternPresentation.defaultReversed(NativeViewport.PATTERN_LINEAR,
                NativeViewport.AXIS_X, seeds));
        assertEquals(2, PatternPresentation.defaultCount(NativeViewport.PATTERN_LINEAR,
                NativeViewport.AXIS_Y, seeds));
        assertTrue("room only toward -Y runs that way",
                PatternPresentation.defaultReversed(NativeViewport.PATTERN_LINEAR,
                        NativeViewport.AXIS_Y, seeds));
        assertEquals("no seed still opens on two", 2, PatternPresentation.defaultCount(
                NativeViewport.PATTERN_LINEAR, NativeViewport.AXIS_Z, seeds));
        assertEquals(6, PatternPresentation.defaultCount(NativeViewport.PATTERN_CIRCULAR,
                NativeViewport.AXIS_Z, seeds));
        assertEquals(2, PatternPresentation.defaultCount(NativeViewport.PATTERN_MIRROR,
                NativeViewport.AXIS_X, seeds));
        assertFalse(PatternPresentation.defaultReversed(NativeViewport.PATTERN_CIRCULAR,
                NativeViewport.AXIS_Y, seeds));
    }

    @Test
    public void countsParseOnlyAsWholeNumbers() {
        assertEquals(4, PatternPresentation.parseCount(" 4 "));
        assertEquals(-1, PatternPresentation.parseCount("2.5"));
        assertEquals(-1, PatternPresentation.parseCount(""));
        assertEquals(NativeViewport.AXIS_Z, PatternPresentation.nextAxis(NativeViewport.AXIS_Y));
        assertEquals(NativeViewport.AXIS_X, PatternPresentation.nextAxis(NativeViewport.AXIS_Z));
    }
}
