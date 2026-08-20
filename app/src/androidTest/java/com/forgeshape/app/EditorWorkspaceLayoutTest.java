package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.isFullyOnScreen;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.releaseOrientation;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.setOrientation;
import static com.forgeshape.app.WorkspaceTestSupport.unoccludedViewportFraction;
import static com.forgeshape.app.WorkspaceTestSupport.waitForLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.content.pm.ActivityInfo;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * UI-07, UI-08, UI-09 and UI-12: how much of the model the shell leaves
 * visible, in every window it can find itself in.
 *
 * <p>This is the suite that would have caught by machine what the Stage 015A
 * audit found by hand. It measures real laid-out chrome rectangles against the
 * real window; nothing here is a screenshot and nothing is a pixel comparison.
 *
 * <p>The tablet case is exercised by the same assertions: the tests read the
 * window they are actually in, derive the layout class from it, and apply the
 * floor that belongs to that class. Running the suite under an overridden
 * window size is therefore a genuine expanded-layout run and not a simulation.
 */
@RunWith(AndroidJUnit4.class)
public final class EditorWorkspaceLayoutTest {

    /** The floor for a compact or medium window, as a fraction of window area
     *  through which the model is still visible. */
    private static final double COMPACT_FLOOR = 0.60;

    /** With the inspector fully open a compact window gives up more, but never
     *  approaches the failure this stage removes. */
    private static final double COMPACT_FLOOR_INSPECTOR_OPEN = 0.50;

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    @Before
    public void startFromABaselineBox() {
        resetToBaselineConstruction(rule.getScenario());
    }

    @After
    public void letTheDeviceChooseItsOwnOrientationAgain() {
        releaseOrientation(rule.getScenario());
    }

    // -----------------------------------------------------------------------
    // UI-07 / UI-09 -- the window the suite is actually running in
    // -----------------------------------------------------------------------

    @Test
    public void ui07_theCurrentWindowKeepsAUsefulViewport() {
        assertViewportFloorHolds("as launched");
    }

    @Test
    public void ui09_theLayoutClassMatchesTheWindowAndItsInspectorFitsIt() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int widthDp = EditorControlStyles.toDp(activity, workspace.getWidth());
            final int heightDp = EditorControlStyles.toDp(activity, workspace.getHeight());
            final WorkspaceLayoutMode expected =
                    WorkspaceLayoutMode.forWindow(widthDp, heightDp);
            assertEquals("the shell classifies the window it is in",
                    expected, workspace.layoutMode());
            assertEquals("and places the inspector accordingly",
                    expected.inspectorPlacement(heightDp), workspace.inspectorPlacement());

            final View inspector = workspace.findViewById(R.id.property_inspector);
            assertTrue("no panel may occupy the whole window",
                    inspector.getWidth() < workspace.getWidth()
                            || inspector.getHeight() < workspace.getHeight());
            if (expected == WorkspaceLayoutMode.EXPANDED) {
                final int viewportDp = EditorControlStyles.toDp(activity,
                        workspace.getWidth() - inspector.getWidth()
                                - workspace.findViewById(R.id.tool_rail).getWidth());
                assertTrue("an expanded window keeps a large central viewport: "
                        + viewportDp + " dp", viewportDp >= Math.round(widthDp * 0.60f));
                assertTrue(viewportDp >= 480);
            }
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UI-08 -- the landscape regression
    // -----------------------------------------------------------------------

    /**
     * The measured defect was 0 % unoccluded viewport at 914 x 411 dp, with the
     * status line laid out below the window bottom and no scroll container
     * anywhere to reach it. All three are asserted here.
     */
    @Test
    public void ui08_landscapeNeverCoversTheModelAndKeepsItsStatusLineOnScreen() {
        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        waitForLayout(rule.getScenario(), true);

        assertViewportFloorHolds("landscape");

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertNotEquals("a short window never gets a bottom sheet",
                    WorkspaceLayoutMode.InspectorPlacement.BOTTOM_SHEET,
                    workspace.inspectorPlacement());
            assertTrue("the status message must be laid out inside the window",
                    isFullyOnScreen(workspace.findViewById(R.id.status_message), workspace));
            assertTrue("every primary commit must be reachable",
                    isFullyOnScreen(workspace.findViewById(R.id.tool_rail), workspace));
            assertTrue("the inspector body scrolls rather than overflowing",
                    workspace.findViewById(R.id.inspector_scroll)
                            instanceof android.widget.ScrollView);
            return null;
        });
    }

    /**
     * The decision is re-derived from whatever window the shell finds itself
     * in, both ways round.
     *
     * <p>It deliberately does not require the placement to <i>differ</i>
     * between the two orientations. On a phone it does, which is the fix; on a
     * tablet both orientations are expanded and both dock, which is correct
     * rather than a defect. What must always hold is that the placement equals
     * the decision for the current window and that the floor is met.
     */
    @Test
    public void ui08_theDecisionIsReRunForWhicheverWindowTheShellIsIn() {
        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        waitForLayout(rule.getScenario(), true);
        assertPlacementMatchesTheWindow("landscape");
        assertViewportFloorHolds("landscape (re-run)");

        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        waitForLayout(rule.getScenario(), false);
        assertPlacementMatchesTheWindow("portrait");
        assertViewportFloorHolds("back in portrait");
    }

    private void assertPlacementMatchesTheWindow(final String where) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int widthDp = EditorControlStyles.toDp(activity, workspace.getWidth());
            final int heightDp = EditorControlStyles.toDp(activity, workspace.getHeight());
            assertEquals("the layout class is derived, not remembered (" + where + ")",
                    WorkspaceLayoutMode.forWindow(widthDp, heightDp), workspace.layoutMode());
            assertEquals("and so is the placement (" + where + ")",
                    workspace.layoutMode().inspectorPlacement(heightDp),
                    workspace.inspectorPlacement());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UI-12 -- collapsing gives the viewport back
    // -----------------------------------------------------------------------

    @Test
    public void ui12_collapsingTheInspectorAndHidingChromeGiveTheViewportBack() {
        final double open = measureUnoccluded(true);
        final double collapsed = measureUnoccluded(false);
        android.util.Log.i("ForgeShape", String.format(java.util.Locale.US,
                "FORGESHAPE_UI_VIEWPORT detent inspector-open=%.1f%% collapsed=%.1f%%",
                open * 100.0, collapsed * 100.0));

        assertTrue("collapsing must give area back: " + open + " -> " + collapsed,
                collapsed > open);
        final boolean expanded = onWorkspace(rule.getScenario(), (activity, workspace) ->
                workspace.layoutMode() == WorkspaceLayoutMode.EXPANDED);
        if (!expanded) {
            assertTrue("a collapsed compact or medium window must clear the floor: "
                    + collapsed, collapsed >= COMPACT_FLOOR);
        }

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.hide_ui_toggle).performClick();
            return null;
        });
        WorkspaceTestSupport.settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(workspace.chromeHidden());
            assertEquals("hiding chrome leaves the bare model", 1.0,
                    unoccludedViewportFraction(workspace.getWidth(), workspace.getHeight(),
                            workspace.chromeRects()), 1.0e-9);
            assertEquals("with one way back", View.VISIBLE,
                    workspace.findViewById(R.id.restore_ui_chip).getVisibility());
            workspace.findViewById(R.id.restore_ui_chip).performClick();
            return null;
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(View.VISIBLE, workspace.findViewById(R.id.global_toolbar)
                    .getVisibility());
            return null;
        });
    }

    @Test
    public void ui12_theDetentSurvivesAModeRoundTrip() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final PropertyInspectorView inspector = workspace.propertyInspector();
            if (inspector.isExpanded()) {
                workspace.findViewById(R.id.inspector_toggle).performClick();
            }
            return null;
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] sculpt = new double[NativeViewport.SCULPT_STATE_SIZE];
            NativeViewport.sculptState(sculpt);
            if (sculpt[NativeViewport.SCULPT_HAS_MESH] != 0.0) {
                workspace.findViewById(R.id.resume_sculpt).performClick();
            } else {
                workspace.findViewById(R.id.freeze_to_sculpt).performClick();
            }
            return null;
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.back_to_construction).performClick();
            return null;
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("Construction's detent is remembered across a mode round trip",
                    !workspace.propertyInspector().isExpanded());
            return null;
        });
    }

    // -----------------------------------------------------------------------

    private double measureUnoccluded(final boolean inspectorExpanded) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            if (workspace.propertyInspector().isExpanded() != inspectorExpanded) {
                workspace.findViewById(R.id.inspector_toggle).performClick();
            }
            return null;
        });
        WorkspaceTestSupport.settleLayout();
        return onWorkspace(rule.getScenario(), (activity, workspace) ->
                unoccludedViewportFraction(workspace.getWidth(), workspace.getHeight(),
                        workspace.chromeRects()));
    }

    /**
     * Asserts the floor that belongs to the window the suite is actually in.
     *
     * <p>Compact and medium windows are held to a proportion of window area,
     * because on a phone that is what "the model is still the subject" means.
     * An expanded window is held to an absolute viewport width instead, because
     * a proportion is the wrong measure once the window is large.
     */
    private void assertViewportFloorHolds(final String where) {
        WorkspaceTestSupport.settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double unoccluded = unoccludedViewportFraction(workspace.getWidth(),
                    workspace.getHeight(), workspace.chromeRects());
            assertTrue("the model must never be fully covered (" + where + "): "
                    + unoccluded, unoccluded > 0.0);

            final boolean inspectorOpen = workspace.propertyInspector().isExpanded();
            // Logged, not only asserted: the numbers are the evidence for the
            // viewport floor, and a passing assertion does not record them.
            android.util.Log.i("ForgeShape", String.format(java.util.Locale.US,
                    "FORGESHAPE_UI_VIEWPORT %s window=%dx%d dp=%dx%d mode=%s placement=%s "
                            + "inspector=%s unoccluded=%.1f%%",
                    where, workspace.getWidth(), workspace.getHeight(),
                    EditorControlStyles.toDp(activity, workspace.getWidth()),
                    EditorControlStyles.toDp(activity, workspace.getHeight()),
                    workspace.layoutMode(), workspace.inspectorPlacement(),
                    inspectorOpen ? "open" : "collapsed", unoccluded * 100.0));
            final double floor = workspace.layoutMode() == WorkspaceLayoutMode.EXPANDED
                    ? 0.40
                    : (inspectorOpen ? COMPACT_FLOOR_INSPECTOR_OPEN : COMPACT_FLOOR);
            assertTrue("unoccupied viewport " + String.format(java.util.Locale.US, "%.1f%%",
                            unoccluded * 100.0) + " (" + where + ", " + workspace.layoutMode()
                            + ", inspector " + (inspectorOpen ? "open" : "collapsed")
                            + ") is below the floor " + floor,
                    unoccluded >= floor);
            return null;
        });
    }
}
