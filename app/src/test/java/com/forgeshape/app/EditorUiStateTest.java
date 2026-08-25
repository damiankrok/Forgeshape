package com.forgeshape.app;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

/**
 * The UI-owned state, checked without a device.
 *
 * <p>What is asserted here is mostly what this class refuses to do: it cannot
 * hold a primitive kind the product does not have, and it does not re-open a
 * panel the user collapsed just because the window changed shape.
 */
public final class EditorUiStateTest {

    @Test
    public void startsInMetersOnABoxShowingTheShapeEditor() {
        final EditorUiState state = new EditorUiState();
        assertEquals(LengthUnit.METERS, state.displayUnit());
        assertEquals(NativeViewport.PRIMITIVE_BOX, state.draftPrimitiveKind());
        assertEquals(EditorUiState.CONSTRUCTION_TOOL_SHAPE, state.constructionTool());
        assertFalse(state.chromeHidden());
    }

    @Test
    public void acceptsEveryRealPrimitiveKind() {
        final EditorUiState state = new EditorUiState();
        for (int kind = NativeViewport.PRIMITIVE_BOX; kind <= NativeViewport.PRIMITIVE_PLANE;
                kind++) {
            state.setDraftPrimitiveKind(kind);
            assertEquals(kind, state.draftPrimitiveKind());
        }
    }

    @Test
    public void refusesAKindTheProductDoesNotHave() {
        final EditorUiState state = new EditorUiState();
        state.setDraftPrimitiveKind(NativeViewport.PRIMITIVE_SPHERE);
        state.setDraftPrimitiveKind(NativeViewport.PRIMITIVE_PLANE + 1);
        assertEquals("falls back to a box rather than pointing at nothing",
                NativeViewport.PRIMITIVE_BOX, state.draftPrimitiveKind());
        state.setDraftPrimitiveKind(-1);
        assertEquals(NativeViewport.PRIMITIVE_BOX, state.draftPrimitiveKind());
    }

    @Test
    public void refusesAConstructionToolThatIsNotOneOfTheTwo() {
        final EditorUiState state = new EditorUiState();
        state.setConstructionTool(EditorUiState.CONSTRUCTION_TOOL_TRANSFORM);
        assertEquals(EditorUiState.CONSTRUCTION_TOOL_TRANSFORM, state.constructionTool());
        state.setConstructionTool(99);
        assertEquals(EditorUiState.CONSTRUCTION_TOOL_SHAPE, state.constructionTool());
    }

    /**
     * UIR4A-01, at the level the rule is actually decided.
     *
     * <p>The resting workspace has no precision surface in it, in either mode
     * and in every window. This is the arithmetic half of "no permanent bottom
     * Inspector"; the instrumentation asserts the other half, that nothing is
     * laid out along the bottom edge as a result.
     */
    @Test
    public void thePrecisionSurfaceStartsClosedInBothModes() {
        final EditorUiState state = new EditorUiState();
        assertFalse("Construction rests on the viewport, not on a panel",
                state.precisionOpen(false));
        assertFalse("Sculpt rests on the viewport, not on a panel",
                state.precisionOpen(true));
    }

    @Test
    public void remembersWhetherThePrecisionSurfaceIsOpenPerMode() {
        final EditorUiState state = new EditorUiState();
        state.setPrecisionOpen(false, true);
        state.setPrecisionOpen(true, false);
        assertTrue("the exact values were asked for in Construction",
                state.precisionOpen(false));
        assertFalse("and were not asked for in Sculpt",
                state.precisionOpen(true));
    }

    /**
     * There is deliberately no window-size rule that opens it.
     *
     * <p>The previous shell opened the panel by itself on a roomy window, so a
     * rotation could put a surface on screen the user had never asked for. The
     * layout decision no longer has an opinion at all — the only thing that
     * opens the precision surface is the precision toggle — which is why the
     * layout mode is passed nothing here and there is nothing to pass it to.
     */
    @Test
    public void aFreshWorkspaceStateHasNoSurfaceOpen() {
        EditorUiState.carryAcrossRecreation(null);
        final EditorUiState fresh = EditorUiState.forNewWorkspace();
        assertFalse(fresh.precisionOpen(false));
        assertFalse(fresh.precisionOpen(true));
    }

    // -----------------------------------------------------------------------
    // The start question
    // -----------------------------------------------------------------------

    @Test
    public void aFreshProcessHasNotYetAnsweredTheStartQuestion() {
        final EditorUiState state = new EditorUiState();
        state.clearStartChoice();
        assertFalse("a process that has not been asked must be asked",
                state.startChoiceMade());
    }

    @Test
    public void answeringTheStartQuestionIsRememberedForTheWholeProcess() {
        final EditorUiState state = new EditorUiState();
        state.clearStartChoice();
        state.recordStartChoice();
        assertTrue(state.startChoiceMade());

        // This is the case that matters, and the reason the flag is process
        // scoped rather than per instance: an Activity recreation builds a
        // whole new workspace and therefore a whole new EditorUiState, and it
        // must NOT put the question back.
        assertTrue("a recreated Activity must not re-ask how the model began",
                new EditorUiState().startChoiceMade());
    }

    @Test
    public void theStartAnswerDoesNotRecordWHICHWayWasChosen() {
        final EditorUiState state = new EditorUiState();
        state.clearStartChoice();
        state.recordStartChoice();

        // Everything this class may remember is layout, drafts or presentation.
        // Which representation the user is in is native truth, read back on
        // every refresh, and a copy of it here would be a second answer that
        // could disagree with the first.
        assertEquals("choosing a start flow must not touch the draft kind",
                NativeViewport.PRIMITIVE_BOX, state.draftPrimitiveKind());
        assertEquals("nor which Construction editor the rail points at",
                EditorUiState.CONSTRUCTION_TOOL_SHAPE, state.constructionTool());
        assertEquals("nor the display unit", LengthUnit.METERS, state.displayUnit());
        assertFalse("nor whether the chrome is hidden", state.chromeHidden());
    }
}
