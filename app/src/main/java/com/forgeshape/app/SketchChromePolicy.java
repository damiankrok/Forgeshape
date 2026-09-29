package com.forgeshape.app;

/**
 * Which drawing chrome a sketch shows in each of its states
 * (`CAD-VERTICAL-SLICE-R1`).
 *
 * <p>A sketch answers two questions one after the other. While it is being
 * DRAWN, the drawing tools, the orientation navigator and a selected Line's
 * dimension belong on screen. Once Finish Sketch has moved it to Ready, the
 * question is the extrusion — which region, how far, which operation — and that
 * is answered at the geometry by the canvas HUD. The drawing chrome then has
 * nothing to act on, so it is ABSENT rather than disabled, and one control,
 * Back to Sketch, returns to it.
 *
 * <p>Pure Java over the native sketch state, so the rule is one statement the
 * views read and a JVM test pins, rather than a predicate repeated in each view.
 * It decides nothing native owns: whether the extrude HUD is live is still the
 * manipulator's own answer.
 */
final class SketchChromePolicy {

    private SketchChromePolicy() {
    }

    /**
     * The Tool Rail. It carries the construction or sculpt entries outside a
     * sketch and the seven drawing tools while one is drawn; in Ready there is
     * nothing to draw on.
     */
    static boolean toolRailShown(int sketchState) {
        return sketchState != NativeViewport.SKETCH_READY;
    }

    /**
     * The orientation navigator: the plane, flip and quarter turns of the
     * AUTHORING view. Ready leaves that view for the feature view, so the
     * navigator goes with it and comes back with Back to Sketch.
     */
    static boolean orientationNavigatorShown(int sketchState) {
        return sketchState == NativeViewport.SKETCH_EDITING;
    }

    /** A selected Line's dimension: an editing annotation, drawing-only. */
    static boolean lineDimensionShown(int sketchState) {
        return sketchState == NativeViewport.SKETCH_EDITING;
    }

    /** Back to Sketch: the one way from the extrusion back to the drawing. */
    static boolean backToSketchShown(int sketchState) {
        return sketchState == NativeViewport.SKETCH_READY;
    }

    /** Cancel: present in both states of an open sketch. */
    static boolean cancelSketchShown(int sketchState) {
        return sketchState != NativeViewport.SKETCH_INACTIVE;
    }

    /**
     * Whether Finish Sketch opens the precision surface by itself. It does not:
     * the extrusion is authored at the geometry, and the exact-value surface
     * is one tap away on its own toggle rather than covering the model
     * unasked.
     */
    static boolean precisionOpensOnFinish() {
        return false;
    }
}
