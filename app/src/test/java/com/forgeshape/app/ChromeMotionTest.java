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

    // -----------------------------------------------------------------------
    // UIR4B-08 — the anchored-surface contract, as far as it can be argued
    // about without a device
    // -----------------------------------------------------------------------

    /**
     * UIR4B-08. The growth is longer than a fade, leaving is quicker than
     * arriving, and both numbers are in the range a surface can be SEEN
     * travelling in.
     *
     * <p>The four surfaces that grow out of a control each carried private
     * copies of these before UI-R4B, and the copies had drifted to three
     * different pairs. What that cost is not abstract: a growth is the only
     * thing that says "this panel belongs to that button", and at the fade
     * durations the copies used, the travel was over before the eye could
     * resolve where it started.
     */
    @Test
    public void uir4b08_theAnchoredGrowthIsLongerThanAFadeAndLeavesQuicker() {
        assertTrue("a growth has to be seen travelling, unlike a fade",
                ChromeMotion.ANCHORED_ENTER_MS > ChromeMotion.ENTER_MS);
        assertTrue("and still has to be over before it is waited on",
                ChromeMotion.ANCHORED_ENTER_MS >= 180L
                        && ChromeMotion.ANCHORED_ENTER_MS <= 220L);
        assertTrue("dismissal has already been decided about",
                ChromeMotion.ANCHORED_EXIT_MS < ChromeMotion.ANCHORED_ENTER_MS);
        assertTrue(ChromeMotion.ANCHORED_EXIT_MS >= 130L
                && ChromeMotion.ANCHORED_EXIT_MS <= 170L);
    }

    /**
     * UIR4B-08. The curve is an ease-OUT, and the scale is uniform.
     *
     * <p>Asserted as the four control points rather than by sampling a built
     * interpolator, so no Android class is loaded — which is also why
     * {@link ChromeMotion#anchoredEase()} builds its curve lazily. What makes it
     * an ease-out is the first control point: its y is already at the top while
     * its x is still near the start, so most of the travel is spent immediately.
     * A symmetric or ease-in curve would put the slow part at the beginning,
     * which is where the user is waiting.
     */
    @Test
    public void uir4b08_theAnchoredCurveIsAnEaseOut() {
        assertTrue("the first control point must lift immediately",
                ChromeMotion.ANCHORED_EASE_Y1 >= 0.9f);
        assertTrue("while its x is still near the start",
                ChromeMotion.ANCHORED_EASE_X1 <= 0.35f);
        assertTrue("and the second must already be settled",
                ChromeMotion.ANCHORED_EASE_Y2 >= 0.9f);
        assertTrue(ChromeMotion.ANCHORED_EASE_X2 > ChromeMotion.ANCHORED_EASE_X1
                && ChromeMotion.ANCHORED_EASE_X2 <= 1.0f);
    }

    /**
     * UIR4B-08. One start scale, applied to both axes.
     *
     * <p>A single constant is the mechanism: two of the copies this replaced
     * scaled X and Y by different amounts, which stretches a panel into place
     * rather than growing it, and there is no way to express that against one
     * number.
     */
    @Test
    public void uir4b08_theAnchoredGrowthStartsFromOneUniformScale() {
        assertTrue("small enough to read as growth",
                ChromeMotion.ANCHORED_START_SCALE < 1.0f);
        assertTrue("large enough that nothing inside is legibly the wrong size",
                ChromeMotion.ANCHORED_START_SCALE >= 0.94f);
    }

    /**
     * UIR4B-10. Reduced motion applies to the anchored growth exactly as it
     * applies to a fade: zero, meaning land on the final state now.
     */
    @Test
    public void uir4b10_reducedMotionZerosTheAnchoredGrowthToo() {
        assertEquals(0L, ChromeMotion.duration(ChromeMotion.ANCHORED_ENTER_MS, 0.0f));
        assertEquals(0L, ChromeMotion.duration(ChromeMotion.ANCHORED_EXIT_MS, 0.0f));
        assertEquals(ChromeMotion.ANCHORED_ENTER_MS,
                ChromeMotion.duration(ChromeMotion.ANCHORED_ENTER_MS, 1.0f));
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
