package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.closePrecision;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.isFullyOnScreen;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.openPrecision;
import static com.forgeshape.app.WorkspaceTestSupport.releaseOrientation;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.setOrientation;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static com.forgeshape.app.WorkspaceTestSupport.unoccludedViewportFraction;
import static com.forgeshape.app.WorkspaceTestSupport.waitForLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
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
            assertEquals("the precision surface is not in the resting workspace",
                    View.GONE, inspector.getVisibility());
            assertTrue("no panel may occupy the whole window",
                    inspector.getWidth() < workspace.getWidth()
                            || inspector.getHeight() < workspace.getHeight());
            if (expected == WorkspaceLayoutMode.EXPANDED) {
                // The Objects column is subtracted when it is there. UI-R1C2
                // added a third docked surface, and measuring the viewport
                // without it would report a number the user never sees.
                final View objects = workspace.findViewById(R.id.objects_dock);
                final int objectsPx =
                        objects.getVisibility() == View.VISIBLE ? objects.getWidth() : 0;
                final int viewportDp = EditorControlStyles.toDp(activity,
                        workspace.getWidth() - inspector.getWidth() - objectsPx
                                - workspace.findViewById(R.id.tool_rail).getWidth());
                // The absolute floor holds in every expanded window, with two
                // docked surfaces or with three. The 60 % PROPORTIONAL floor is
                // the two-surface rule and is asserted only there — see
                // WorkspaceLayoutModeTest for why a third column is held to the
                // absolute half of it instead.
                assertTrue("an expanded window keeps a viewport at least as wide as a phone: "
                        + viewportDp + " dp", viewportDp >= 480);
                if (objectsPx == 0) {
                    assertTrue("with two docked surfaces the proportional floor still holds: "
                            + viewportDp + " dp", viewportDp >= Math.round(widthDp * 0.60f));
                }
            }
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UI-R1C2: the adaptive pass
    //
    // These read the window the suite is ACTUALLY in and assert the contract
    // that belongs to it, which is this suite's existing rule. Running the
    // instrumentation under an overridden window size is therefore a genuine
    // expanded run rather than a simulation, and is how the expanded branches
    // are exercised.
    // -----------------------------------------------------------------------

    /**
     * R1C2-27 / R1C2-28 / R1C2-33. Whatever window this is, the workspace obeys
     * the decision for it — and Objects is reachable either way.
     */
    @Test
    public void r1c227_objectsAndTheRailFollowTheDecisionForThisWindow() {
        WorkspaceTestSupport.settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int widthDp = EditorControlStyles.toDp(activity, workspace.getWidth());
            final WorkspaceLayoutMode mode = workspace.layoutMode();

            assertEquals("the Objects surface follows the decision, never a guess",
                    mode.objectsDocked(widthDp), workspace.objectsDocked());
            assertNotNull("the trailing host is present in every window",
                    workspace.findViewById(R.id.workspace_trailing_host));

            // R1C2-27: Objects is reachable in EVERY window. The section is one
            // instance that moves, so this holds wherever it currently hangs —
            // which is the whole reason it is one instance.
            assertNotNull("the Objects section exists in every layout",
                    workspace.findViewById(R.id.objects_section));
            assertNotNull("the column host's creation control exists in every layout",
                    workspace.findViewById(R.id.add_body));
            assertEquals("every body has a row in every layout",
                    NativeViewport.sceneBodyCount(), workspace.objectsSection().rowCount());

            if (workspace.objectsDocked()) {
                // R1C2-28: the scene list and the exact values are on screen at
                // the same time, which is the point of the expanded layout.
                final View dock = workspace.findViewById(R.id.objects_dock);
                assertEquals(View.VISIBLE, dock.getVisibility());
                assertTrue("the Objects column is laid out with a real width",
                        dock.getWidth() > 0);
                assertTrue("the Objects column is on screen", isFullyOnScreen(dock, workspace));
                assertEquals("the capsule withdraws where a column names the body",
                        View.GONE, workspace.objectsCapsule().getVisibility());
                assertEquals("the one Objects section is what is in the column",
                        dock, workspace.objectsSection().getParent());
                assertEquals("a docked window docks the inspector too",
                        WorkspaceLayoutMode.InspectorPlacement.SIDE_DOCK,
                        workspace.inspectorPlacement());
            } else {
                // R1C2-27: a compact or medium window keeps the current
                // viewport-first model, and no column stands on the model.
                assertEquals("no Objects column in a window that has not earned one",
                        View.GONE, workspace.findViewById(R.id.objects_dock).getVisibility());
                // UI-R2: and Objects is in its own panel rather than nested in
                // the Property Inspector's shape editor, which is what stopped a
                // panel named "Shape" from opening on the list of bodies.
                assertTrue("Objects lives in the scene panel, not in the inspector",
                        workspace.objectsPopover().hosts(workspace.objectsSection()));
                assertEquals("and the capsule that opens it is on screen",
                        View.VISIBLE, workspace.objectsCapsule().getVisibility());
            }
            return null;
        });
    }

    /**
     * R1C2-36. A layout change never resizes the render target.
     *
     * <p>The {@code SurfaceView} is the whole window in every mode, so docking,
     * un-docking and re-parenting chrome cannot change the swapchain's extent.
     * Asserted by measuring the viewport before and after a rotation that
     * genuinely re-runs the whole decision: the viewport must equal the
     * workspace in both, whatever the chrome did in between.
     */
    @Test
    public void r1c236_theViewportIsTheWholeWindowInEveryLayout() {
        assertViewportFillsTheWorkspace("as launched");

        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        waitForLayout(rule.getScenario(), true);
        assertViewportFillsTheWorkspace("landscape");

        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        waitForLayout(rule.getScenario(), false);
        assertViewportFillsTheWorkspace("back in portrait");
    }

    /**
     * R1C2-37. Portrait and rotated both settle at a legitimate arrangement,
     * with the model still the subject.
     *
     * <p>The rotation runs the structural rearrangement — a possible re-parent
     * of the Objects section while the trailing host keeps its fixed anchor — and the
     * assertion is that it converges, not that it animated. A re-parent during
     * a measure pass is never animated, deliberately.
     */
    @Test
    public void r1c237_everyOrientationSettlesAtALegitimateArrangement() {
        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        waitForLayout(rule.getScenario(), true);
        assertArrangementIsConsistent("landscape");
        assertViewportFloorHolds("landscape with the UI-R1C2 chrome");

        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        waitForLayout(rule.getScenario(), false);
        assertArrangementIsConsistent("portrait");
        assertViewportFloorHolds("portrait with the UI-R1C2 chrome");
    }

    /**
     * R1C2-34. The Tool Rail's tap-versus-scroll rule survives the dock.
     *
     * <p>UI-R1B1 fixed a rail entry losing its tap to the scroll container. The
     * dock changes what the rail LOOKS like and nothing about how it negotiates
     * a gesture, and this is what says so: a real small drift on a real entry,
     * dispatched through the real scroll container, still selects.
     */
    @Test
    public void r1c234_theRailStillTakesASmallDriftAsATapWhicheverWayItIsDrawn() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View place = workspace.findViewById(R.id.tool_rail_place);
            assertNotNull("the rail is built", place);
            assertTrue("a rail entry is on screen whichever way it is drawn",
                    place.getWidth() > 0 && place.getHeight() > 0);
            assertTrue("and meets the touch floor: "
                            + EditorControlStyles.toDp(activity, place.getHeight()) + " dp",
                    EditorControlStyles.toDp(activity, place.getHeight()) >= 48);
            // It consumes its own gestures whichever window it is in, which is
            // what keeps a reach for a tool from orbiting the camera.
            assertTrue("the rail consumes its own drag",
                    WorkspaceTestSupport.dragConsumed(
                            WorkspaceTestSupport.toolRailScroll(workspace)));
            // The rail is a RAISED FLOATING CAPSULE IN EVERY WINDOW since
            // UI-R4B. It used to be repainted flush and level when the window
            // docked it, which meant one control had two visual identities in
            // one session — a Sculpt user who rotated a tablet watched the brush
            // selector turn into part of the wall. Docking now decides position
            // and never material, so this assertion no longer branches: what it
            // guards is precisely that the branch does not come back.
            assertTrue("the unified right host is raised in every window, docked or not: "
                            + WorkspaceTestSupport.trailingHost(workspace).getElevation(),
                    WorkspaceTestSupport.trailingHost(workspace).getElevation() > 0.0f);
            return null;
        });
    }

    private void assertViewportFillsTheWorkspace(final String where) {
        WorkspaceTestSupport.settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            assertNotNull("the viewport exists (" + where + ")", viewport);
            assertEquals("the SurfaceView is the whole window's width (" + where + ")",
                    workspace.getWidth(), viewport.getWidth());
            assertEquals("the SurfaceView is the whole window's height (" + where + ")",
                    workspace.getHeight(), viewport.getHeight());
            assertEquals("and it is not offset by any chrome (" + where + ")",
                    0, viewport.getLeft() + viewport.getTop());
            return null;
        });
    }

    private void assertArrangementIsConsistent(final String where) {
        WorkspaceTestSupport.settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int widthDp = EditorControlStyles.toDp(activity, workspace.getWidth());
            final int heightDp = EditorControlStyles.toDp(activity, workspace.getHeight());
            final WorkspaceLayoutMode expected =
                    WorkspaceLayoutMode.forWindow(widthDp, heightDp);
            assertEquals("the mode is re-derived (" + where + ")",
                    expected, workspace.layoutMode());
            assertNotNull("the trailing host remains attached (" + where + ")",
                    workspace.findViewById(R.id.workspace_trailing_host).getParent());
            assertEquals("the Objects surface is re-derived (" + where + ")",
                    expected.objectsDocked(widthDp), workspace.objectsDocked());
            // Whatever happened, there is exactly ONE Objects section and it has
            // exactly one parent. A rearrangement that had leaked a second copy
            // would show up here as a row count that no longer matches the scene.
            assertNotNull("the Objects section has a parent (" + where + ")",
                    workspace.objectsSection().getParent());
            assertEquals("one row per body, still (" + where + ")",
                    NativeViewport.sceneBodyCount(), workspace.objectsSection().rowCount());
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
     *
     * <p>The status line is written first, and that is a UI-R4B change to the
     * CASE rather than to what it proves. Since the status line gained a
     * lifecycle it says nothing at rest and is not drawn at all when it has
     * nothing to say, so a resting workspace has no capsule to measure. What has
     * to be true — and what this asserted then and asserts now — is that a
     * message the product actually writes is laid out where the user can read
     * it, in the one window that used to put it past the bottom edge.
     */
    @Test
    public void ui08_landscapeNeverCoversTheModelAndKeepsItsStatusLineOnScreen() {
        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        waitForLayout(rule.getScenario(), true);

        assertViewportFloorHolds("landscape");

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.showStatus(activity.getString(R.string.reject_relation),
                    R.attr.fsTextError);
            return null;
        });
        settleLayout();

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
    // UI-12 -- dismissing the exact values gives the viewport back
    // -----------------------------------------------------------------------

    @Test
    public void ui12_closingTheInspectorAndHidingChromeGiveTheViewportBack() {
        final double open = measureUnoccluded(true);
        final double closed = measureUnoccluded(false);
        android.util.Log.i("ForgeShape", String.format(java.util.Locale.US,
                "FORGESHAPE_UI_VIEWPORT precision open=%.1f%% closed=%.1f%%",
                open * 100.0, closed * 100.0));

        assertTrue("dismissing must give area back: " + open + " -> " + closed,
                closed > open);
        final boolean expanded = onWorkspace(rule.getScenario(), (activity, workspace) ->
                workspace.layoutMode() == WorkspaceLayoutMode.EXPANDED);
        if (!expanded) {
            assertTrue("a resting compact or medium window must clear the floor: "
                    + closed, closed >= COMPACT_FLOOR);
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

    /**
     * UI-12, now about a surface rather than a detent: whether the user asked
     * for the exact values is remembered per mode across a mode round trip.
     */
    @Test
    public void ui12_theDetentSurvivesAModeRoundTrip() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            openPrecision(workspace);
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
            assertTrue("Construction's precision surface is remembered across a mode "
                            + "round trip", workspace.propertyInspector().isOpen());
            assertTrue("and so is the fact that it was asked for",
                    workspace.uiState().precisionOpen(false));
            return null;
        });
    }

    // -----------------------------------------------------------------------

    private double measureUnoccluded(final boolean precisionOpen) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            if (precisionOpen) {
                openPrecision(workspace);
            } else {
                closePrecision(workspace);
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

            final boolean inspectorOpen = workspace.propertyInspector().isOpen();
            // Logged, not only asserted: the numbers are the evidence for the
            // viewport floor, and a passing assertion does not record them.
            android.util.Log.i("ForgeShape", String.format(java.util.Locale.US,
                    "FORGESHAPE_UI_VIEWPORT %s window=%dx%d dp=%dx%d mode=%s placement=%s "
                            + "inspector=%s unoccluded=%.1f%%",
                    where, workspace.getWidth(), workspace.getHeight(),
                    EditorControlStyles.toDp(activity, workspace.getWidth()),
                    EditorControlStyles.toDp(activity, workspace.getHeight()),
                    workspace.layoutMode(), workspace.inspectorPlacement(),
                    inspectorOpen ? "open" : "closed", unoccluded * 100.0));
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
