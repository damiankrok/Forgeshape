package com.forgeshape.app;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

/**
 * R1C1-09 — the shared motion contract, argued about without a device.
 *
 * <p>Only the decisions are tested here, because only the decisions are
 * testable off-device: how long a transition may take, and whether it may run
 * at all. Whether a view actually ended up transparent is a view question and
 * lives in the instrumented suite; asserting it here would need a fake View and
 * would prove nothing about the real one.
 *
 * <p>What makes this worth writing is that {@link ChromeMotion#duration}
 * returns <b>0</b> rather than a small number for reduced motion. Every caller
 * branches on that zero to land on the final state instead of running a short
 * animation, so if it ever returned 1 ms instead, every reduced-motion promise
 * in the product would quietly become false while still looking correct.
 */
public class ChromeMotionTest {

    @Test
    public void r1c109_theTwoDurationsAreShortAndLeavingIsQuickerThanArriving() {
        assertTrue("a chrome transition is feedback, not an event to wait out",
                ChromeMotion.ENTER_MS <= 150L);
        assertTrue("what is going away has already been decided about",
                ChromeMotion.EXIT_MS < ChromeMotion.ENTER_MS);
        assertTrue(ChromeMotion.EXIT_MS > 0L);
    }

    @Test
    public void r1c109_aZeroAnimatorScaleMeansDoNotAnimateAtAll() {
        assertFalse(ChromeMotion.animationsEnabled(0.0f));
        assertEquals("0 is the caller's signal to land on the final state now",
                0L, ChromeMotion.duration(ChromeMotion.ENTER_MS, 0.0f));
        assertEquals(0L, ChromeMotion.duration(ChromeMotion.EXIT_MS, 0.0f));
    }

    @Test
    public void r1c109_anOrdinaryScaleRunsTheFullDurationUnscaled() {
        assertTrue(ChromeMotion.animationsEnabled(1.0f));
        // The platform applies the scale to every ViewPropertyAnimator itself.
        // Applying it here as well would slow every transition by the square of
        // the user's setting.
        assertEquals(ChromeMotion.ENTER_MS, ChromeMotion.duration(ChromeMotion.ENTER_MS, 1.0f));
        assertEquals(ChromeMotion.ENTER_MS, ChromeMotion.duration(ChromeMotion.ENTER_MS, 0.5f));
        assertEquals(ChromeMotion.ENTER_MS, ChromeMotion.duration(ChromeMotion.ENTER_MS, 10.0f));
    }

    @Test
    public void r1c109_anImpossibleScaleReducesMotionRatherThanAnimating() {
        // No platform should report either of these, but nothing prevents it,
        // and the safe answer to "I do not understand this setting" is to
        // animate less rather than more.
        assertFalse(ChromeMotion.animationsEnabled(Float.NaN));
        assertFalse(ChromeMotion.animationsEnabled(-1.0f));
        assertEquals(0L, ChromeMotion.duration(ChromeMotion.ENTER_MS, Float.NaN));
    }
}
