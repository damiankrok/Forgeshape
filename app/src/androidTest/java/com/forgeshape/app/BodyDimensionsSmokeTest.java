package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.view.View;
import android.widget.EditText;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * `E2E-DIM020M-01`: the one device journey for Stage 020M.
 *
 * <h2>What this suite is for, and what it deliberately is not</h2>
 *
 * <p>The DOMAIN half — that a dimension is the body's own local extent times
 * its stored Absolute Scale, that rotation does not move it, that each anchor
 * holds the world point it claims to hold on a turned body, that invalid and
 * degenerate requests fail closed, that one commit is one exact Undo, that a
 * Relative Scale multiplier is spent rather than stored, and that none of it
 * changes a `.forge` byte — is proved by `DIM020M-01..16` in the body-dimension
 * self-test suite, which builds its own scenes and costs milliseconds.
 *
 * <p>What is left, and all this covers, is the single journey that can only be
 * true on a device: the two controls exist under Transform, entering Dimensions
 * actually withdraws the transform gizmo, an exact value typed into the
 * viewport label reaches the domain in centre mode, changing the anchor and
 * typing again moves the body the other way, and Relative Scale opens at 1/1/1
 * again after a commit. It is ONE flow over ONE primitive on purpose: repeating
 * it per primitive, per axis, per anchor and per palette would re-prove what
 * the domain suite already establishes, and Stage 020M runs under a
 * reduced-testing policy (`TEST-OWNER-03`).
 */
@RunWith(AndroidJUnit4.class)
public final class BodyDimensionsSmokeTest {

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private byte[] baselineProject;

    @Before
    public void setUp() {
        resetToBaselineConstruction(rule.getScenario());
        baselineProject = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.encodeProject());
    }

    @After
    public void tearDown() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.setBodyDimensionsMode(false);
            NativeViewport.enterConstructionMode();
            if (baselineProject != null) {
                NativeViewport.loadProject(baselineProject);
            }
            return null;
        });
        resetToBaselineConstruction(rule.getScenario());
    }

    @Test
    public void e2eDim020m01_dimensionsAndRelativeScaleRunThroughTheRealControls() {
        enterTransform();

        // --- The two controls exist, and only under Transform --------------
        assertTrue("Transform offers Dimensions", isShown(R.id.body_dimensions));
        assertTrue("Transform offers Relative Scale", isShown(R.id.body_relative_scale));
        assertMeetsTouchFloor(R.id.body_dimensions, "Dimensions");
        assertMeetsTouchFloor(R.id.body_relative_scale, "Relative scale");
        assertTrue("the transform gizmo is up before Dimensions is entered", gizmoActive());

        // --- Entering Dimensions hides the gizmo --------------------------
        click(R.id.body_dimensions);
        assertTrue("Dimensions mode is open", state(NativeViewport.BODY_DIM_MODE_ACTIVE) != 0.0);
        assertFalse("the normal transform gizmo is withdrawn in Dimensions mode", gizmoActive());
        assertTrue("the anchor selector arrives with the mode",
                isShown(R.id.dimension_anchor_group));
        assertEquals("and it opens on centre", NativeViewport.DIMENSION_ANCHOR_CENTER,
                (int) state(NativeViewport.BODY_DIM_ANCHOR));
        assertTrue("the dimension labels stand in the viewport",
                isShown(R.id.body_dimension_labels));

        // --- One exact dimension, in centre mode --------------------------
        final double[] before = placement();
        final int undoBefore = undoDepth();
        typeDimension(0, "3");
        assertEquals("the X dimension is what was typed", 3.0,
                state(NativeViewport.BODY_DIM_X), 1e-9);
        assertEquals("centre writes no position", before[NativeViewport.TRANSFORM_POSITION],
                placement()[NativeViewport.TRANSFORM_POSITION], 1e-9);
        assertEquals("one exact dimension edit is one history step", undoBefore + 1, undoDepth());

        // --- Change the anchor, and edit the same axis again ---------------
        click(R.id.dimension_anchor_negative);
        assertEquals("the anchor moved to the negative side",
                NativeViewport.DIMENSION_ANCHOR_NEGATIVE,
                (int) state(NativeViewport.BODY_DIM_ANCHOR));
        final double[] beforeAnchored = placement();
        final int undoBeforeAnchored = undoDepth();
        typeDimension(0, "6");
        assertEquals("the X dimension is what was typed again", 6.0,
                state(NativeViewport.BODY_DIM_X), 1e-9);
        // A one-sided anchor MOVES the body, which is exactly the difference
        // from the centre edit above.
        assertTrue("a one-sided anchor moves the placement",
                Math.abs(beforeAnchored[NativeViewport.TRANSFORM_POSITION]
                        - placement()[NativeViewport.TRANSFORM_POSITION]) > 1e-9);
        assertEquals("and it is still one history step", undoBeforeAnchored + 1, undoDepth());

        // --- Relative Scale, then reopen -----------------------------------
        click(R.id.body_relative_scale);
        assertFalse("opening Relative Scale closes Dimensions mode",
                state(NativeViewport.BODY_DIM_MODE_ACTIVE) != 0.0);
        assertEquals("the multiplier opens at 1", "1", multiplierText(0));
        assertEquals("on every axis", "1", multiplierText(1));
        assertEquals("on every axis", "1", multiplierText(2));

        final double scaleBefore = state(NativeViewport.BODY_DIM_SCALE_X);
        final int undoBeforeRelative = undoDepth();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.relativeScaleEditor().rowFor(R.id.field_relative_scale_x).setText("2");
            workspace.relativeScaleEditor().commitControl().performClick();
            return null;
        });
        settleLayout();
        assertEquals("the stored Absolute Scale is the product", scaleBefore * 2.0,
                state(NativeViewport.BODY_DIM_SCALE_X), 1e-9);
        assertEquals("one Apply is one history step", undoBeforeRelative + 1, undoDepth());

        // Reopening starts at the identity again: the multiplier was spent, not
        // stored, and there is nothing anywhere that could hand it back.
        click(R.id.body_relative_scale);
        assertEquals("reopening shows 1", "1", multiplierText(0));
        assertEquals("reopening shows 1", "1", multiplierText(1));
        assertEquals("reopening shows 1", "1", multiplierText(2));
    }

    // -----------------------------------------------------------------------
    // Helpers. Every control is reached by its semantic id, never by a screen
    // coordinate: the workspace re-arranges itself per window.
    // -----------------------------------------------------------------------

    /** Types an exact dimension into the viewport label for one axis. */
    private void typeDimension(final int axis, final String value) {
        final int labelId = axis == 0 ? R.id.body_dimension_label_x
                : axis == 1 ? R.id.body_dimension_label_y : R.id.body_dimension_label_z;
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View label = workspace.findViewById(labelId);
            assertNotNull("the axis carries a numeric label", label);
            label.performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("the editor opened on the axis that was tapped", axis,
                    workspace.bodyDimensionLabels().editingAxis());
            final EditText field = (EditText) workspace.findViewById(R.id.field_body_dimension);
            assertNotNull("tapping a value opens a field", field);
            field.setText(value);
            workspace.findViewById(R.id.apply_body_dimension).performClick();
            return null;
        });
        settleLayout();
    }

    private String multiplierText(final int axis) {
        final int id = axis == 0 ? R.id.field_relative_scale_x
                : axis == 1 ? R.id.field_relative_scale_y : R.id.field_relative_scale_z;
        return onWorkspace(rule.getScenario(), (activity, workspace) ->
                workspace.relativeScaleEditor().rowFor(id).text());
    }

    private void enterTransform() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.uiState().setConstructionTool(EditorUiState.CONSTRUCTION_TOOL_TRANSFORM);
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    private void click(final int id) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View control = workspace.findViewById(id);
            assertNotNull("the control is in the window", control);
            control.performClick();
            return null;
        });
        settleLayout();
    }

    private boolean isShown(final int id) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View control = workspace.findViewById(id);
            return control != null && control.getVisibility() == View.VISIBLE;
        });
    }

    private void assertMeetsTouchFloor(final int id, final String what) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View view = workspace.findViewById(id);
            assertNotNull(what + " is in the window", view);
            final float density = activity.getResources().getDisplayMetrics().density;
            final int floor = Math.round(48f * density);
            assertTrue(what + " meets the 48 dp floor: " + view.getWidth() + "x"
                            + view.getHeight(),
                    view.getWidth() >= floor - 1 && view.getHeight() >= floor - 1);
            assertNotNull(what + " says what it does", view.getContentDescription());
            return null;
        });
    }

    private double state(final int slot) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] out = new double[NativeViewport.BODY_DIM_SIZE];
            NativeViewport.bodyDimensionsState(out);
            return out[slot];
        });
    }

    private double[] placement() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] out = new double[NativeViewport.TRANSFORM_SIZE];
            NativeViewport.boxTransform(out);
            return out;
        });
    }

    private boolean gizmoActive() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] out = new double[NativeViewport.GIZMO_STATE_SIZE];
            NativeViewport.gizmoState(out);
            return out[NativeViewport.GIZMO_ACTIVE] != 0.0;
        });
    }

    private int undoDepth() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.constructionUndoDepth());
    }
}
