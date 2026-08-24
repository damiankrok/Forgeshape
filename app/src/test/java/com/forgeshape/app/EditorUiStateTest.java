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
        state.setConstructionTool(EditorUiState.CONSTRUCTION_TOOL_PLACE);
        assertEquals(EditorUiState.CONSTRUCTION_TOOL_PLACE, state.constructionTool());
        state.setConstructionTool(99);
        assertEquals(EditorUiState.CONSTRUCTION_TOOL_SHAPE, state.constructionTool());
    }

    @Test
    public void remembersTheInspectorDetentPerMode() {
        final EditorUiState state = new EditorUiState();
        state.setInspectorExpanded(false, true);
        state.setInspectorExpanded(true, false);
        assertTrue("Construction is where the exact values are typed",
                state.inspectorExpanded(false));
        assertFalse("Sculpt's inspector is a status surface and stays out of the way",
                state.inspectorExpanded(true));
    }

    @Test
    public void appliesTheOpeningDetentOnceAndThenLeavesTheUserAlone() {
        final EditorUiState state = new EditorUiState();
        state.applyInitialDetents(WorkspaceLayoutMode.COMPACT, 914);
        assertFalse(state.inspectorExpanded(false));

        // The user opens it, then the window is rotated into a shape whose
        // opening detent would be different. The user's choice must stand.
        state.setInspectorExpanded(false, true);
        state.applyInitialDetents(WorkspaceLayoutMode.MEDIUM, 411);
        assertTrue("a rotation must not undo a deliberate collapse or expand",
                state.inspectorExpanded(false));
    }

    @Test
    public void aRoomyWindowOpensTheInspector() {
        final EditorUiState state = new EditorUiState();
        state.applyInitialDetents(WorkspaceLayoutMode.EXPANDED, 800);
        assertTrue(state.inspectorExpanded(false));
        assertTrue(state.inspectorExpanded(true));
    }

    @Test
    public void aShortMediumWindowStillOpensCollapsed() {
        final EditorUiState state = new EditorUiState();
        state.applyInitialDetents(WorkspaceLayoutMode.MEDIUM, 411);
        assertFalse(state.inspectorExpanded(false));
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
