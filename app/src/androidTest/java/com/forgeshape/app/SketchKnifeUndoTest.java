package com.forgeshape.app;

import static com.forgeshape.app.SketchTestSupport.dragSketch;
import static com.forgeshape.app.SketchTestSupport.selectTool;
import static com.forgeshape.app.SketchTestSupport.sketchEntityCount;
import static com.forgeshape.app.SketchTestSupport.sketchState;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * `SKETCH-KNIFE-UNDO-R1` on the device: the history capsule's Undo and Redo
 * step the staged sketch while it is drawn, and the knife on the Tool Rail
 * cuts every stroke one swipe crosses, as one Undo step. Every act goes through
 * a real control or a real Down / Move / Up on the viewport.
 *
 * <p>Push-through to Cut is a native drag rule over the frozen axis and is
 * proved on the host (`SKU_PT_*`), where the ray and the axis are exact.
 */
@RunWith(AndroidJUnit4.class)
public final class SketchKnifeUndoTest {

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    @Before
    public void startAtHome() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.dismissRecoveryPromptForTest();
            workspace.showHomeAsFirstLaunchForTest();
            return null;
        });
        settleLayout();
    }

    @After
    public void returnToABaselineProject() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.supportChooserCancel();
            NativeViewport.sketchCancel();
            NativeViewport.enterConstructionMode();
            return null;
        });
        resetToBaselineConstruction(rule.getScenario());
    }

    @Test
    public void sku01_undoAndRedoStepTheSketchWhileItIsDrawn() {
        enterFirstSketch();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("the pair is drawn in the sketch", View.VISIBLE,
                    workspace.historyGroup().getVisibility());
            assertFalse("nothing drawn, nothing to undo", workspace.undoAction().isEnabled());
            assertFalse(workspace.redoAction().isEnabled());
            return null;
        });
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), -1.0, -1.0, 1.0, 1.0);
        assertEquals(1, sketchEntityCount());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("one drawn rectangle can be undone", workspace.undoAction().isEnabled());
            return null;
        });
        press(R.id.undo_action);
        assertEquals("Undo took the rectangle back", 0, sketchEntityCount());
        assertEquals("and the sketch is still open", NativeViewport.SKETCH_EDITING, sketchState());
        assertEquals("no Construction step was taken", 0, NativeViewport.constructionUndoDepth());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("Redo is offered", workspace.redoAction().isEnabled());
            return null;
        });
        press(R.id.redo_action);
        assertEquals("Redo put it back", 1, sketchEntityCount());

        // Ready has no sketch step to take: the pair is withdrawn there.
        press(R.id.finish_sketch);
        assertEquals(NativeViewport.SKETCH_READY, sketchState());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("the pair is withdrawn in Ready", View.GONE,
                    workspace.historyGroup().getVisibility());
            return null;
        });
    }

    @Test
    public void sku02_theKnifeOnTheRailCutsEveryStrokeOneSwipeCrosses() {
        enterFirstSketch();
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        dragSketch(rule.getScenario(), -1.0, 0.5, 1.0, 0.5);
        dragSketch(rule.getScenario(), -1.0, -0.5, 1.0, -0.5);
        assertEquals("two separate lines", 2, sketchEntityCount());

        selectTool(rule.getScenario(), R.id.tool_rail_trim);
        assertEquals("the knife is a sketch tool", NativeViewport.SKETCH_TOOL_TRIM,
                NativeViewport.sketchTool());
        final double[] draft = new double[NativeViewport.SKETCH_DRAFT_SIZE];
        NativeViewport.sketchDraftingState(draft);
        assertEquals("and holding it is the Trim mode", NativeViewport.MODIFY_TRIM,
                (int) draft[NativeViewport.SKETCH_DRAFT_MODE]);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View knife = workspace.findViewById(R.id.tool_rail_trim);
            assertNotNull(knife);
            assertTrue("the knife is on the rail", knife.isShown());
            return null;
        });

        // One vertical swipe across both: neither meets another stroke, so
        // each is cut whole.
        dragSketch(rule.getScenario(), 0.0, 1.0, 0.0, -1.0);
        assertEquals("one swipe cut both lines", 0, sketchEntityCount());
        assertEquals("and the sketch is still open", NativeViewport.SKETCH_EDITING, sketchState());

        press(R.id.undo_action);
        assertEquals("the swipe was ONE Undo step", 2, sketchEntityCount());

        selectTool(rule.getScenario(), R.id.tool_rail_line);
        NativeViewport.sketchDraftingState(draft);
        assertEquals("a drawing tool puts the knife down", NativeViewport.MODIFY_NONE,
                (int) draft[NativeViewport.SKETCH_DRAFT_MODE]);
    }

    /** Home → New Project → CAD, which lands straight in the sketch. */
    private void enterFirstSketch() {
        press(R.id.home_new_project);
        press(R.id.new_project_cad);
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
    }

    private void press(final int id) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View control = workspace.findViewById(id);
            assertNotNull("control " + id + " must exist", control);
            assertTrue("control " + id + " must be on screen", control.isShown());
            control.performClick();
            return null;
        });
        settleLayout();
    }
}
