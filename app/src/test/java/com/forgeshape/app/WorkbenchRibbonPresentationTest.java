package com.forgeshape.app;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

/** `WORKBENCH-RIBBON-R2`: what the tablet's ribbon draws is one function over native facts. */
public final class WorkbenchRibbonPresentationTest {

    private static WorkbenchRibbonPresentation.State state(boolean project, boolean sketching,
                                                           boolean sculpting, int faceActs) {
        return new WorkbenchRibbonPresentation.State(project, sketching, sculpting, false,
                false, 0, false, 0, true, faceActs, true, true, true);
    }

    @Test
    public void theRibbonStandsOnAWorkbenchOverAProjectOutsideASketchAndSculpt() {
        assertTrue(WorkbenchRibbonPresentation.shown(true, state(true, false, false, 0)));
        assertFalse("a phone has no ribbon",
                WorkbenchRibbonPresentation.shown(false, state(true, false, false, 0)));
        assertFalse("Home has no ribbon",
                WorkbenchRibbonPresentation.shown(true, state(false, false, false, 0)));
        assertFalse("a sketch owns the viewport and its tools are the rail's",
                WorkbenchRibbonPresentation.shown(true, state(true, true, false, 0)));
        assertFalse("Sculpt's brushes are the rail's",
                WorkbenchRibbonPresentation.shown(true, state(true, false, true, 0)));
    }

    @Test
    public void theFaceGroupDrawsExactlyTheActsTheFaceOffers() {
        final WorkbenchRibbonPresentation.State none = state(true, false, false, 0);
        assertFalse(WorkbenchRibbonPresentation.faceGroupShown(none));
        final WorkbenchRibbonPresentation.State some = state(true, false, false,
                WorkbenchRibbonPresentation.FACE_SKETCH_HERE | WorkbenchRibbonPresentation.FACE_HOLE);
        assertTrue(WorkbenchRibbonPresentation.faceGroupShown(some));
        assertTrue(WorkbenchRibbonPresentation.faceActShown(some,
                WorkbenchRibbonPresentation.ACT_SKETCH_HERE));
        assertTrue(WorkbenchRibbonPresentation.faceActShown(some,
                WorkbenchRibbonPresentation.ACT_HOLE));
        assertFalse("no finish offered, so neither Round nor Bevel is drawn",
                WorkbenchRibbonPresentation.faceActShown(some,
                        WorkbenchRibbonPresentation.ACT_FILLET));
        assertFalse(WorkbenchRibbonPresentation.faceActShown(some,
                WorkbenchRibbonPresentation.ACT_MIRROR));
    }

    @Test
    public void transformCommandsReadTheGizmoAndScaleHasNoSpace() {
        final WorkbenchRibbonPresentation.State move = new WorkbenchRibbonPresentation.State(
                true, false, false, true, true, NativeViewport.GIZMO_MODE_MOVE, true,
                NativeViewport.GIZMO_SPACE_WORLD, false, 0, true, true, true);
        assertTrue(WorkbenchRibbonPresentation.transformModeActive(move,
                NativeViewport.GIZMO_MODE_MOVE));
        assertFalse(WorkbenchRibbonPresentation.transformModeActive(move,
                NativeViewport.GIZMO_MODE_SCALE));
        assertTrue(WorkbenchRibbonPresentation.spaceShown(move));
        assertTrue(WorkbenchRibbonPresentation.transformSpaceActive(move,
                NativeViewport.GIZMO_SPACE_WORLD));
        assertTrue("the primitive can become a part",
                WorkbenchRibbonPresentation.partGroupShown(move));
        assertFalse("the last body is never deleted",
                WorkbenchRibbonPresentation.deleteShown(move));
        final WorkbenchRibbonPresentation.State scale = new WorkbenchRibbonPresentation.State(
                true, false, false, false, true, NativeViewport.GIZMO_MODE_SCALE, false,
                NativeViewport.GIZMO_SPACE_LOCAL, true, 0, true, true, true);
        assertFalse("a world-axis scale of a turned body is a shear: no space to choose",
                WorkbenchRibbonPresentation.spaceShown(scale));
        assertTrue(WorkbenchRibbonPresentation.deleteShown(scale));
        assertEquals(0, WorkbenchRibbonPresentation.TAB_MODEL);
        assertEquals(1, WorkbenchRibbonPresentation.TAB_VIEW);
    }
}
