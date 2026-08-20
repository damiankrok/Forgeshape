package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.describeSnapshotDifference;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.view.View;
import android.widget.EditText;

import androidx.lifecycle.Lifecycle;
import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * UI-14: what the shell shows after the app has been away.
 *
 * <p>Moving the scenario to CREATED and back to RESUMED is the HOME/resume
 * path: the Surface is destroyed and recreated, native state is untouched, and
 * the shell has to rebuild itself from that state rather than from whatever was
 * last tapped.
 */
@RunWith(AndroidJUnit4.class)
public final class EditorWorkspaceLifecycleTest {

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    @Before
    public void startFromABaselineBox() {
        resetToBaselineConstruction(rule.getScenario());
    }

    @Test
    public void ui14_resumingInConstructionRestoresTheConstructionSurfaces() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionCone(1.5, 3.0);
            workspace.syncFromNative();
            return null;
        });

        final double[] before = onWorkspace(rule.getScenario(),
                (activity, workspace) -> WorkspaceTestSupport.nativeSnapshot());

        backgroundAndResume();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.MODE_CONSTRUCTION, NativeViewport.productMode());
            assertNotNull("the Construction rail comes back",
                    workspace.findViewById(R.id.tool_rail_shape));
            assertNull(workspace.findViewById(R.id.tool_rail_grab));
            assertTrue("the object's own kind is what the chooser shows",
                    workspace.findViewById(R.id.primitive_option_cone).isActivated());
            assertEquals("and its exact values are read back, not remembered", "1.5",
                    ((EditText) workspace.findViewById(R.id.field_cone_diameter)).getText()
                            .toString());
            assertEquals("3",
                    ((EditText) workspace.findViewById(R.id.field_cone_height)).getText()
                            .toString());

            final double[] after = WorkspaceTestSupport.nativeSnapshot();
            assertArrayEquals("nothing about the object may change across a resume:"
                    + describeSnapshotDifference(before, after), before, after, 0.0);
            return null;
        });
    }

    @Test
    public void ui14_resumingInSculptRestoresTheSculptSurfacesAndTheBrush() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] sculpt = new double[NativeViewport.SCULPT_STATE_SIZE];
            NativeViewport.sculptState(sculpt);
            workspace.findViewById(sculpt[NativeViewport.SCULPT_HAS_MESH] != 0.0
                    ? R.id.resume_sculpt : R.id.freeze_to_sculpt).performClick();
            return null;
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.tool_rail_smooth).performClick();
            return null;
        });

        final double[] before = onWorkspace(rule.getScenario(),
                (activity, workspace) -> WorkspaceTestSupport.nativeSnapshot());

        backgroundAndResume();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("the mode is read back from native truth",
                    NativeViewport.MODE_SCULPT, NativeViewport.productMode());
            assertEquals(View.VISIBLE,
                    workspace.findViewById(R.id.brush_edge_controls).getVisibility());
            assertTrue("the tool that survived is the one drawn active",
                    workspace.findViewById(R.id.tool_rail_smooth).isActivated());
            assertEquals(NativeViewport.TOOL_SMOOTH, NativeViewport.sculptTool());
            assertNotNull(workspace.findViewById(R.id.freeze_again));

            final double[] after = WorkspaceTestSupport.nativeSnapshot();
            assertArrayEquals("the Frozen Sculpt Mesh and the brush survive untouched:"
                    + describeSnapshotDifference(before, after), before, after, 0.0);
            return null;
        });
    }

    /**
     * The mode on screen follows native truth even when the request was
     * refused, which is why it is read back rather than assumed.
     */
    @Test
    public void ui14_aRefusedModeRequestLeavesTheCorrectSurfacesOnScreen() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int result = NativeViewport.enterSculptMode();
            workspace.syncFromNative();
            final boolean sculpting =
                    NativeViewport.productMode() == NativeViewport.MODE_SCULPT;
            assertEquals("the shell shows the mode native code is in, not the one asked for",
                    sculpting, result == NativeViewport.SCULPT_OK);
            assertEquals(sculpting ? View.VISIBLE : View.GONE,
                    workspace.findViewById(R.id.brush_edge_controls).getVisibility());
            return null;
        });
    }

    private void backgroundAndResume() {
        rule.getScenario().moveToState(Lifecycle.State.CREATED);
        settleLayout();
        rule.getScenario().moveToState(Lifecycle.State.RESUMED);
        settleLayout();
    }
}
