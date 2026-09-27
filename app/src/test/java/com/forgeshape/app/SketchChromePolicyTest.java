package com.forgeshape.app;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

/**
 * `CAD-VERTICAL-SLICE-R1` §7.6 on the JVM: which drawing chrome a sketch
 * shows while it is drawn and which it withdraws once it is Ready.
 *
 * <p>The device proves the views obey it
 * ({@code CadVerticalSliceTest.ui_context_withdrawal}); this pins the rule
 * itself, in every state, without one.
 */
public final class SketchChromePolicyTest {

    @Test
    public void drawingShowsTheDrawingChrome() {
        final int editing = NativeViewport.SKETCH_EDITING;
        assertTrue("the drawing tools", SketchChromePolicy.toolRailShown(editing));
        assertTrue("the orientation navigator",
                SketchChromePolicy.orientationNavigatorShown(editing));
        assertTrue("a selected Line's dimension", SketchChromePolicy.lineDimensionShown(editing));
        assertTrue("Cancel", SketchChromePolicy.cancelSketchShown(editing));
        assertFalse("and no Back to Sketch: this IS the sketch",
                SketchChromePolicy.backToSketchShown(editing));
    }

    @Test
    public void readyWithdrawsTheDrawingChromeAndOffersOneWayBack() {
        final int ready = NativeViewport.SKETCH_READY;
        assertFalse("the drawing tools are absent", SketchChromePolicy.toolRailShown(ready));
        assertFalse("the navigator is absent",
                SketchChromePolicy.orientationNavigatorShown(ready));
        assertFalse("the Line dimension is absent", SketchChromePolicy.lineDimensionShown(ready));
        assertTrue("Back to Sketch is the way back", SketchChromePolicy.backToSketchShown(ready));
        assertTrue("and Cancel stays", SketchChromePolicy.cancelSketchShown(ready));
    }

    @Test
    public void outsideASketchNothingOfItIsDrawnAndTheRailIsTheWorkspaces() {
        final int inactive = NativeViewport.SKETCH_INACTIVE;
        assertTrue("the rail carries the workspace's own entries",
                SketchChromePolicy.toolRailShown(inactive));
        assertFalse(SketchChromePolicy.orientationNavigatorShown(inactive));
        assertFalse(SketchChromePolicy.lineDimensionShown(inactive));
        assertFalse(SketchChromePolicy.backToSketchShown(inactive));
        assertFalse(SketchChromePolicy.cancelSketchShown(inactive));
    }

    @Test
    public void finishSketchLeavesThePrecisionSurfaceCollapsed() {
        assertFalse("the exact-value surface is on demand, never opened unasked",
                SketchChromePolicy.precisionOpensOnFinish());
    }
}
