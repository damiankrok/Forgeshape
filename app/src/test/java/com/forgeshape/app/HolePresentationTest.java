package com.forgeshape.app;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

/**
 * The hole editor's presentation rules, on the JVM (`CAD-HOLE-R1`): which
 * rows each kind draws, and that every seed a NEW hole opens with is one native
 * accepts — a counterbore wider than the bore and shallower than a blind hole,
 * a cone wider than the bore whose depth fits above the bottom. A device
 * proves the panel obeys it; this proves the rule itself.
 */
public final class HolePresentationTest {

    @Test
    public void eachKindDrawsOnlyItsOwnRow() {
        assertFalse(HolePresentation.boreShown(NativeViewport.HOLE_SIMPLE));
        assertFalse(HolePresentation.sinkShown(NativeViewport.HOLE_SIMPLE));
        assertTrue(HolePresentation.boreShown(NativeViewport.HOLE_COUNTERBORE));
        assertFalse(HolePresentation.sinkShown(NativeViewport.HOLE_COUNTERBORE));
        assertTrue(HolePresentation.sinkShown(NativeViewport.HOLE_COUNTERSINK));
        assertFalse(HolePresentation.boreShown(NativeViewport.HOLE_COUNTERSINK));
    }

    @Test
    public void niceValuesAreOneTwoOrFiveTimesAPowerOfTen() {
        assertEquals(0.2, HolePresentation.niceAtLeast(0.16), 1e-12);
        assertEquals(0.5, HolePresentation.niceAtLeast(0.32), 1e-12);
        assertEquals(1.0, HolePresentation.niceAtLeast(0.8), 1e-12);
        assertEquals(0.005, HolePresentation.niceAtLeast(0.005), 1e-15);
        assertEquals(0.0, HolePresentation.niceAtLeast(0.0), 0.0);
        assertEquals(0.0, HolePresentation.niceAtLeast(Double.NaN), 0.0);
    }

    @Test
    public void everySeedIsOneNativeAccepts() {
        final double half = Math.tan(Math.toRadians(HolePresentation.DEFAULT_SINK_ANGLE / 2.0));
        for (double d : new double[] {0.0001, 0.003, 0.005, 0.01, 0.2, 0.3, 0.5, 2.0, 7.0}) {
            final double depth = HolePresentation.defaultDepth(d);
            final double bore = HolePresentation.defaultBoreDiameter(d);
            final double boreDepth = HolePresentation.defaultBoreDepth(d);
            final double sink = HolePresentation.defaultSinkDiameter(d);
            final double sinkDepth = 0.5 * (sink - d) / half;
            assertTrue("a counterbore is wider than its hole at " + d, bore > d);
            assertTrue("and shallower than a blind hole at " + d, boreDepth < depth);
            assertTrue("a cone is wider than its hole at " + d, sink > d);
            assertTrue("and ends above a blind hole's bottom at " + d, sinkDepth < depth);
        }
        assertTrue(HolePresentation.DEFAULT_SINK_ANGLE >= 30.0
                && HolePresentation.DEFAULT_SINK_ANGLE <= 150.0);
    }

    @Test
    public void kindsHaveDistinctNames() {
        assertTrue(HolePresentation.kindName(NativeViewport.HOLE_SIMPLE)
                != HolePresentation.kindName(NativeViewport.HOLE_COUNTERBORE));
        assertTrue(HolePresentation.kindName(NativeViewport.HOLE_COUNTERBORE)
                != HolePresentation.kindName(NativeViewport.HOLE_COUNTERSINK));
    }
}
