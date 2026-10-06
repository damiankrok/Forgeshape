package com.forgeshape.app;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

/**
 * The Freeform cage context surface's decisions, on the JVM
 * ({@code MODELING-FOUNDATIONS-R1} B): when cage editing is wanted, and that a
 * control that cannot succeed for the current selection is not drawn.
 */
public class FreeformPresentationTest {

    private static FreeformPresentation.State state(boolean active, int element, int selected,
                                                    int faces, int symmetry) {
        final double[] s = new double[NativeViewport.FREEFORM_STATE_SIZE];
        s[NativeViewport.FREEFORM_STATE_ACTIVE] = active ? 1.0 : 0.0;
        s[NativeViewport.FREEFORM_STATE_ACTIVE_IS_FREEFORM] = 1.0;
        s[NativeViewport.FREEFORM_STATE_EDITABLE] = 1.0;
        s[NativeViewport.FREEFORM_STATE_ELEMENT] = element;
        s[NativeViewport.FREEFORM_STATE_SELECTION_COUNT] = selected;
        s[NativeViewport.FREEFORM_STATE_FACES] = faces;
        s[NativeViewport.FREEFORM_STATE_SYMMETRY] = symmetry;
        s[NativeViewport.FREEFORM_STATE_LEVEL] = 2;
        return new FreeformPresentation.State(s);
    }

    @Test
    public void cageEditingIsWantedOnlyUnderShapeOverAnEditableFreeformBody() {
        final FreeformPresentation.State s = state(false, NativeViewport.FREEFORM_ELEMENT_FACE, 0, 6, 0);
        assertTrue(FreeformPresentation.editWanted(true, false, false, true, s));
        assertFalse("not under Transform", FreeformPresentation.editWanted(true, false, false, false, s));
        assertFalse("not in Sculpt", FreeformPresentation.editWanted(true, true, false, true, s));
        assertFalse("not while sketching", FreeformPresentation.editWanted(true, false, true, true, s));
        assertFalse("not at Home", FreeformPresentation.editWanted(false, false, false, true, s));
        final double[] locked = new double[NativeViewport.FREEFORM_STATE_SIZE];
        locked[NativeViewport.FREEFORM_STATE_ACTIVE_IS_FREEFORM] = 1.0;
        assertFalse("not over a locked or hidden body",
                FreeformPresentation.editWanted(true, false, false, true, new FreeformPresentation.State(locked)));
    }

    @Test
    public void faceToolsNeedAFaceSelection() {
        final FreeformPresentation.State none = state(true, NativeViewport.FREEFORM_ELEMENT_FACE, 0, 6, 0);
        final FreeformPresentation.State one = state(true, NativeViewport.FREEFORM_ELEMENT_FACE, 1, 6, 0);
        final FreeformPresentation.State edges = state(true, NativeViewport.FREEFORM_ELEMENT_EDGE, 1, 6, 0);
        assertFalse(FreeformPresentation.showPushPull(none));
        assertFalse(FreeformPresentation.showExtrude(none));
        assertTrue(FreeformPresentation.showPushPull(one));
        assertTrue(FreeformPresentation.showExtrude(one));
        assertTrue(FreeformPresentation.showDeleteFaces(one));
        assertFalse("every face cannot go",
                FreeformPresentation.showDeleteFaces(state(true, NativeViewport.FREEFORM_ELEMENT_FACE, 6, 6, 0)));
        assertFalse(FreeformPresentation.showPushPull(edges));
    }

    @Test
    public void edgeToolsNeedAnEdgeSelectionAndALoopExactlyOne() {
        final FreeformPresentation.State one = state(true, NativeViewport.FREEFORM_ELEMENT_EDGE, 1, 6, 0);
        final FreeformPresentation.State two = state(true, NativeViewport.FREEFORM_ELEMENT_EDGE, 2, 6, 0);
        assertTrue(FreeformPresentation.showInsertLoop(one));
        assertFalse(FreeformPresentation.showInsertLoop(two));
        assertTrue(FreeformPresentation.showCrease(two));
        assertFalse(FreeformPresentation.showCrease(state(true, NativeViewport.FREEFORM_ELEMENT_VERTEX, 2, 6, 0)));
    }

    @Test
    public void transformModesNeedASelectionAndNothingShowsOutsideEditing() {
        assertTrue(FreeformPresentation.showTransformModes(
                state(true, NativeViewport.FREEFORM_ELEMENT_VERTEX, 1, 6, 0)));
        assertFalse(FreeformPresentation.showTransformModes(
                state(true, NativeViewport.FREEFORM_ELEMENT_VERTEX, 0, 6, 0)));
        final FreeformPresentation.State closed = state(false, NativeViewport.FREEFORM_ELEMENT_FACE, 1, 6, 0);
        assertFalse(FreeformPresentation.showPushPull(closed));
        assertFalse(FreeformPresentation.showTransformModes(closed));
    }

    @Test
    public void symmetryTogglesOnePlaneAndKeepsTheOthers() {
        final FreeformPresentation.State xz = state(true, NativeViewport.FREEFORM_ELEMENT_FACE, 0, 6,
                NativeViewport.FREEFORM_SYMMETRY_X | NativeViewport.FREEFORM_SYMMETRY_Z);
        assertTrue(FreeformPresentation.symmetryOn(xz, NativeViewport.FREEFORM_SYMMETRY_X));
        assertFalse(FreeformPresentation.symmetryOn(xz, NativeViewport.FREEFORM_SYMMETRY_Y));
        assertEquals(NativeViewport.FREEFORM_SYMMETRY_Z,
                FreeformPresentation.toggledSymmetry(xz, NativeViewport.FREEFORM_SYMMETRY_X));
        assertEquals(0x07, FreeformPresentation.toggledSymmetry(xz, NativeViewport.FREEFORM_SYMMETRY_Y));
    }

    @Test
    public void everyRefusalHasAReadableSentence() {
        assertEquals(R.string.freeform_refused_selection,
                FreeformPresentation.refusalMessage(NativeViewport.FREEFORM_EMPTY_SELECTION));
        assertEquals(R.string.freeform_refused_midpoint, FreeformPresentation.refusalMessage(29));
        assertEquals(R.string.freeform_refused_loop, FreeformPresentation.refusalMessage(31));
        assertEquals(R.string.freeform_refused_mode,
                FreeformPresentation.refusalMessage(NativeViewport.FREEFORM_REFUSED_IN_SCULPT));
        assertEquals(R.string.freeform_refused_other, FreeformPresentation.refusalMessage(999));
    }
}
