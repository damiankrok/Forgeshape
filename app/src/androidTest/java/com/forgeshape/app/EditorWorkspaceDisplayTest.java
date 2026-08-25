package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.describeSnapshotDifference;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.nativeSnapshot;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
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
 * SHD-13, SHD-15 and SHD-16: the display control's stable ids, and the promise
 * that everything behind them is presentation only.
 *
 * <p>Every control is reached by its stable semantic id. No assertion here
 * depends on where anything is on screen, and none asserts a rendered pixel —
 * whether Studio Solid actually looks like clay is the renderer's business,
 * proven by the native shading suite and by runtime evidence, not from Java.
 */
@RunWith(AndroidJUnit4.class)
public final class EditorWorkspaceDisplayTest {

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    @Before
    public void startFromABaselineBox() {
        resetToBaselineConstruction(rule.getScenario());
    }

    /**
     * Display settings are process-scoped native state, so a test that leaves
     * MatCap selected would hand it to the next test. Restoring the product
     * defaults is this suite's own responsibility.
     */
    @After
    public void restoreDisplayDefaults() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.setShadingModel(NativeViewport.SHADING_STUDIO);
            NativeViewport.setSurfaceShading(NativeViewport.SURFACE_SMOOTH);
            NativeViewport.setProjectionMode(NativeViewport.PROJECTION_PERSPECTIVE);
            NativeViewport.setGridVisible(true);
            // The viewport appearance too: r1c222 drives it directly to prove
            // the grid is orthogonal to it, and a case that died part-way
            // through would otherwise hand a cream viewport to a Dark-theme
            // suite that runs next.
            NativeViewport.setViewportBackground(NativeViewport.VIEWPORT_BACKGROUND_WARM_GRAPHITE);
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // UI-R1C2: the View group and the world reference grid
    //
    // R1C2-17 and R1C2-19..25. What a grid LOOKS like is not asserted here, for
    // exactly the reason nothing else in this suite asserts a pixel: that is
    // judged by the native presentation suite and by runtime evidence. What is
    // asserted is that the control exists, is bounded, reads back from native
    // truth, survives everything it must survive, and cannot reach the model.
    // -----------------------------------------------------------------------

    /** R1C2-17. The View group exists, works, and is exactly one control. */
    @Test
    public void r1c217_theDisplayPopoverCarriesABoundedViewGroup() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.display_settings_button).performClick();
            return null;
        });
        // The popover starts GONE and has never been laid out, so its chips
        // report a height of 0 until a traversal has actually run. Measuring in
        // the same block that opens it is the same mistake the popover's own
        // first-open pivot bug was — see DisplaySettingsPopoverView.onSizeChanged.
        WorkspaceTestSupport.settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View on = workspace.findViewById(R.id.view_grid_on);
            final View off = workspace.findViewById(R.id.view_grid_off);
            assertNotNull("the View group offers Grid On", on);
            assertNotNull("the View group offers Grid Off", off);

            // Both are real controls, not drawn promises. UI-R1C2 shows nothing
            // it cannot honour, so nothing in this group may be disabled.
            assertTrue("Grid On is a working control", on.isEnabled());
            assertTrue("Grid Off is a working control", off.isEnabled());

            // Bounded: the group is the grid and nothing else. A View group that
            // had quietly grown a Selection Outline, a View Cube, Named Views or
            // any snapping control would fail here, which is the scope guard.
            assertEquals("no Selection Outline control exists yet", 0,
                    activity.getResources().getIdentifier(
                            "view_selection_outline", "id", activity.getPackageName()));
            assertEquals("no View Cube control exists yet", 0,
                    activity.getResources().getIdentifier(
                            "view_cube", "id", activity.getPackageName()));
            assertEquals("no snapping control exists yet", 0,
                    activity.getResources().getIdentifier(
                            "view_snap_to_grid", "id", activity.getPackageName()));

            // The touch floor, measured rather than trusted to the declared
            // size — the R1B1-10b rule.
            assertTrue("Grid On meets the 44 dp touch floor: "
                            + EditorControlStyles.toDp(activity, on.getHeight()) + " dp",
                    EditorControlStyles.toDp(activity, on.getHeight()) >= 44);
            assertTrue("Grid Off meets the 44 dp touch floor: "
                            + EditorControlStyles.toDp(activity, off.getHeight()) + " dp",
                    EditorControlStyles.toDp(activity, off.getHeight()) >= 44);

            // The popover must stay a compact surface rather than becoming a
            // settings screen. Five small groups still fit well inside the
            // window it hangs in.
            final View popover = workspace.findViewById(R.id.display_settings_popover);
            assertTrue("the popover is not a full-screen settings surface: "
                            + popover.getHeight() + " of " + workspace.getHeight(),
                    popover.getHeight() < workspace.getHeight() * 0.85);
            return null;
        });
    }

    /** R1C2-17. The chips report native truth, both ways, and stay in place. */
    @Test
    public void r1c217_theGridChipsReadBackFromNativeTruth() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.display_settings_button).performClick();

            workspace.findViewById(R.id.view_grid_off).performClick();
            assertTrue("native code reports the grid off", !NativeViewport.gridVisible());
            assertTrue("and Off shows as active",
                    workspace.findViewById(R.id.view_grid_off).isActivated());
            assertTrue("while On does not",
                    !workspace.findViewById(R.id.view_grid_on).isActivated());
            // Selection feedback happens IN PLACE: the popover does not close on
            // a choice, because deciding whether a floor helps means switching
            // back and forth.
            assertEquals("the popover stays open across a choice", View.VISIBLE,
                    workspace.findViewById(R.id.display_settings_popover).getVisibility());

            workspace.findViewById(R.id.view_grid_on).performClick();
            assertTrue("native code reports the grid on", NativeViewport.gridVisible());
            assertTrue(workspace.findViewById(R.id.view_grid_on).isActivated());
            assertTrue(!workspace.findViewById(R.id.view_grid_off).isActivated());

            // Idempotent: asking for what is already in effect is inert.
            assertTrue("setting the value it already has is honoured, not refused",
                    NativeViewport.setGridVisible(true));
            return null;
        });
    }

    /**
     * R1C2-19. Toggling the grid touches no geometry truth whatsoever.
     *
     * <p>The native snapshot carries the dimensions, the placement, the mesh
     * revision, the sculpt revision and the GPU upload counters. A grid toggle
     * that had rebuilt or re-uploaded a body's render mesh would move one of
     * them; four toggles leave every value byte-identical.
     */
    @Test
    public void r1c219_togglingTheGridChangesNoDomainStateAndUploadsNothing() {
        final double[] before = nativeSnapshot();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.display_settings_button).performClick();
            workspace.findViewById(R.id.view_grid_off).performClick();
            workspace.findViewById(R.id.view_grid_on).performClick();
            workspace.findViewById(R.id.view_grid_off).performClick();
            workspace.findViewById(R.id.view_grid_on).performClick();
            return null;
        });
        final double[] after = nativeSnapshot();
        assertArrayEquals("a grid toggle is presentation only: "
                + describeSnapshotDifference(before, after), before, after, 0.0);
    }

    /** R1C2-20 / R1C2-21. It survives a recreation and a HOME/resume. */
    @Test
    public void r1c220_theGridSurvivesRecreationAndResume() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.setGridVisible(false);
            return null;
        });

        // A recreation is what a rotation and a theme change both come down to.
        // The store is process-scoped, so the value is not restored — it was
        // never lost.
        rule.getScenario().recreate();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("the grid choice outlives the Activity", !NativeViewport.gridVisible());
            workspace.findViewById(R.id.display_settings_button).performClick();
            assertTrue("and the rebuilt control agrees with it",
                    workspace.findViewById(R.id.view_grid_off).isActivated());
            return null;
        });

        rule.getScenario().moveToState(androidx.lifecycle.Lifecycle.State.CREATED);
        rule.getScenario().moveToState(androidx.lifecycle.Lifecycle.State.RESUMED);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("and a HOME/resume", !NativeViewport.gridVisible());
            workspace.findViewById(R.id.display_settings_button).performClick();
            assertTrue(workspace.findViewById(R.id.view_grid_off).isActivated());
            NativeViewport.setGridVisible(true);
            return null;
        });
    }

    /**
     * R1C2-22..25. The grid is orthogonal to appearance and to projection.
     *
     * <p>Neither can refuse it and it can change neither of them: the grid's
     * colours are derived from the viewport appearance inside native code, and
     * its one draw composes whichever projection the camera reports. Asserted
     * as state rather than as pixels — that both appearances are actually
     * READABLE is a native palette check plus runtime evidence.
     */
    @Test
    public void r1c222_theGridIsIndependentOfAppearanceAndProjection() {
        final double[] before = nativeSnapshot();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int[] backgrounds = {NativeViewport.VIEWPORT_BACKGROUND_WARM_GRAPHITE,
                    NativeViewport.VIEWPORT_BACKGROUND_NEUTRAL_CHARCOAL,
                    NativeViewport.VIEWPORT_BACKGROUND_LIGHT_CHARCOAL};
            final int[] projections = {NativeViewport.PROJECTION_PERSPECTIVE,
                    NativeViewport.PROJECTION_ORTHOGRAPHIC};
            for (int background : backgrounds) {
                for (int projection : projections) {
                    NativeViewport.setViewportBackground(background);
                    NativeViewport.setProjectionMode(projection);
                    for (boolean grid : new boolean[]{true, false, true}) {
                        assertEquals("the grid is honoured in every appearance and projection",
                                grid, NativeViewport.setGridVisible(grid));
                        assertEquals("and does not disturb the appearance",
                                background, NativeViewport.viewportBackground());
                        assertEquals("or the projection",
                                projection, NativeViewport.projectionMode());
                    }
                }
            }
            return null;
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.setViewportBackground(NativeViewport.VIEWPORT_BACKGROUND_WARM_GRAPHITE);
            NativeViewport.setProjectionMode(NativeViewport.PROJECTION_PERSPECTIVE);
            NativeViewport.setGridVisible(true);
            return null;
        });
        final double[] after = nativeSnapshot();
        assertArrayEquals("none of that touched the model: "
                + describeSnapshotDifference(before, after), before, after, 0.0);
    }

    // -----------------------------------------------------------------------
    // SHD-15 -- the controls exist and are reachable by stable id
    // -----------------------------------------------------------------------

    @Test
    public void shd15_displayControlsHaveStableIds() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertNotNull("the Global Toolbar carries the display control",
                    workspace.findViewById(R.id.display_settings_button));

            workspace.findViewById(R.id.display_settings_button).performClick();

            assertNotNull("the display popover has a stable id",
                    workspace.findViewById(R.id.display_settings_popover));
            assertNotNull("Studio Solid is reachable by id",
                    workspace.findViewById(R.id.display_mode_studio));
            assertNotNull("MatCap is reachable by id",
                    workspace.findViewById(R.id.display_mode_matcap));
            assertNotNull("Smooth is reachable by id",
                    workspace.findViewById(R.id.surface_shading_smooth));
            assertNotNull("Faceted is reachable by id",
                    workspace.findViewById(R.id.surface_shading_faceted));
            assertNotNull("Perspective is reachable by id",
                    workspace.findViewById(R.id.projection_perspective));
            assertNotNull("Orthographic is reachable by id",
                    workspace.findViewById(R.id.projection_orthographic));
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // PROJ-13 / PROJ-12 / PROJ-14 -- the camera projection control
    //
    // Projection shares this surface because it is the other control that
    // changes how the object READS without changing what it is. These cases
    // hold it to the same three promises the shading chips make: reachable by a
    // stable id, provably inert against domain state, and native-owned so it
    // survives a resume.
    // -----------------------------------------------------------------------

    @Test
    public void proj13_theProjectionControlSwitchesTheCameraAndShowsWhatIsActive() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.display_settings_button).performClick();

            assertEquals("Perspective is the product default",
                    NativeViewport.PROJECTION_PERSPECTIVE, NativeViewport.projectionMode());
            assertTrue("the Perspective chip starts active",
                    workspace.findViewById(R.id.projection_perspective).isActivated());

            workspace.findViewById(R.id.projection_orthographic).performClick();
            assertEquals("tapping Orthographic switches the camera",
                    NativeViewport.PROJECTION_ORTHOGRAPHIC, NativeViewport.projectionMode());
            assertTrue("the Orthographic chip repaints as active",
                    workspace.findViewById(R.id.projection_orthographic).isActivated());
            assertTrue("and Perspective stops being active",
                    !workspace.findViewById(R.id.projection_perspective).isActivated());

            // Selection feedback happens in place: choosing a projection must
            // not dismiss the panel, because comparing the two means switching
            // back and forth.
            assertEquals("the popover stays open across a projection change", View.VISIBLE,
                    workspace.findViewById(R.id.display_settings_popover).getVisibility());

            workspace.findViewById(R.id.projection_perspective).performClick();
            assertEquals("and back again", NativeViewport.PROJECTION_PERSPECTIVE,
                    NativeViewport.projectionMode());
            return null;
        });
    }

    @Test
    public void proj13_anUnknownProjectionIndexIsRefused() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.setProjectionMode(NativeViewport.PROJECTION_ORTHOGRAPHIC);
            assertEquals("an out-of-range projection is refused",
                    NativeViewport.PROJECTION_ORTHOGRAPHIC, NativeViewport.setProjectionMode(5));
            assertEquals("a negative projection is refused",
                    NativeViewport.PROJECTION_ORTHOGRAPHIC, NativeViewport.setProjectionMode(-1));
            return null;
        });
    }

    @Test
    public void proj12_projectionChangeTouchesNoDomainState() {
        final double[] before = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.display_settings_button).performClick();
            workspace.findViewById(R.id.projection_orthographic).performClick();
            workspace.findViewById(R.id.projection_perspective).performClick();
            workspace.findViewById(R.id.projection_orthographic).performClick();
            return null;
        });

        final double[] after = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());
        // A projection switch is a camera act. It must not move a Construction
        // parameter, a transform, a MeshRevision, a SculptRevision or a vertex
        // count — the object is the same object, seen differently.
        assertArrayEquals("Perspective<->Orthographic must not change any Construction, "
                        + "transform or sculpt state:" + describeSnapshotDifference(before, after),
                before, after, 0.0);
    }

    @Test
    public void proj14_projectionSurvivesHomeAndResume() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.display_settings_button).performClick();
            workspace.findViewById(R.id.projection_orthographic).performClick();
            return null;
        });

        rule.getScenario().moveToState(androidx.lifecycle.Lifecycle.State.CREATED);
        rule.getScenario().moveToState(androidx.lifecycle.Lifecycle.State.RESUMED);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("Orthographic survives a resume",
                    NativeViewport.PROJECTION_ORTHOGRAPHIC, NativeViewport.projectionMode());
            workspace.findViewById(R.id.display_settings_button).performClick();
            assertTrue("the Orthographic chip shows as active after a resume",
                    workspace.findViewById(R.id.projection_orthographic).isActivated());
            return null;
        });
    }

    @Test
    public void shd15_popoverOpensAndCloses() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View button = workspace.findViewById(R.id.display_settings_button);
            final View popover = workspace.findViewById(R.id.display_settings_popover);

            assertEquals("the popover starts closed", View.GONE, popover.getVisibility());
            button.performClick();
            assertEquals("tapping Display opens it", View.VISIBLE, popover.getVisibility());
            button.performClick();
            // The close animation ends asynchronously, so what is asserted here
            // is that the panel is on its way out — alpha or visibility — not
            // that the final GONE has already landed.
            assertTrue("tapping Display again dismisses it",
                    popover.getVisibility() == View.GONE || popover.getAlpha() < 1.0f);
            return null;
        });
    }

    @Test
    public void shd15_hidingChromeAlsoClosesThePopover() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.display_settings_button).performClick();
            assertEquals(View.VISIBLE,
                    workspace.findViewById(R.id.display_settings_popover).getVisibility());

            // "Show me the bare model" must clear the popover too; it lives in
            // the overlay, which deliberately survives chrome being hidden.
            workspace.setChromeHidden(true);
            assertEquals("hiding chrome leaves no display panel behind", View.GONE,
                    workspace.findViewById(R.id.display_settings_popover).getVisibility());

            workspace.setChromeHidden(false);
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // SHD-13 -- presentation only
    // -----------------------------------------------------------------------

    @Test
    public void shd13_shadingModelChangeTouchesNoDomainState() {
        final double[] before = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.display_settings_button).performClick();
            workspace.findViewById(R.id.display_mode_matcap).performClick();
            workspace.findViewById(R.id.display_mode_studio).performClick();
            workspace.findViewById(R.id.display_mode_matcap).performClick();
            return null;
        });

        final double[] after = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());
        assertArrayEquals("Studio<->MatCap must not change any Construction, transform "
                        + "or sculpt state:" + describeSnapshotDifference(before, after),
                before, after, 0.0);
    }

    @Test
    public void shd13_surfaceShadingChangeTouchesNoDomainState() {
        final double[] before = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.display_settings_button).performClick();
            workspace.findViewById(R.id.surface_shading_faceted).performClick();
            workspace.findViewById(R.id.surface_shading_smooth).performClick();
            return null;
        });

        final double[] after = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());
        // Smooth<->Faceted DOES rebuild render-only geometry. That rebuild must
        // still be invisible here: no primitive parameter, no transform, no
        // sculpt revision and no vertex count may move.
        assertArrayEquals("Smooth<->Faceted must not change any Construction, transform "
                        + "or sculpt state:" + describeSnapshotDifference(before, after),
                before, after, 0.0);
    }

    @Test
    public void shd13_displayChangeDoesNotDiscardAHalfTypedDimension() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final android.widget.EditText width = workspace.findViewById(R.id.field_box_width);
            assertNotNull("the box width field is on screen", width);
            width.setText("7.25");

            workspace.findViewById(R.id.display_settings_button).performClick();
            workspace.findViewById(R.id.display_mode_matcap).performClick();
            workspace.findViewById(R.id.surface_shading_faceted).performClick();

            // A display change must not call syncFromNative(): that rewrites the
            // exact-value editors from native truth and would silently throw
            // away a dimension the user was in the middle of typing.
            assertEquals("a display change must leave a half-typed value alone",
                    "7.25", width.getText().toString());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // SHD-16 -- the settings survive a resume
    // -----------------------------------------------------------------------

    @Test
    public void shd16_displaySettingsSurviveHomeAndResume() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.display_settings_button).performClick();
            workspace.findViewById(R.id.display_mode_matcap).performClick();
            workspace.findViewById(R.id.surface_shading_faceted).performClick();
            return null;
        });

        // Through the real lifecycle, not a simulated one.
        rule.getScenario().moveToState(androidx.lifecycle.Lifecycle.State.CREATED);
        rule.getScenario().moveToState(androidx.lifecycle.Lifecycle.State.RESUMED);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("MatCap survives a resume", NativeViewport.SHADING_MATCAP,
                    NativeViewport.shadingModel());
            assertEquals("Faceted survives a resume", NativeViewport.SURFACE_FACETED,
                    NativeViewport.surfaceShading());

            // And the popover agrees with native truth rather than with
            // whatever it last drew: the settings are native-owned, so the UI
            // has to read them back.
            workspace.findViewById(R.id.display_settings_button).performClick();
            assertTrue("the MatCap chip shows as active after a resume",
                    workspace.findViewById(R.id.display_mode_matcap).isActivated());
            assertTrue("the Faceted chip shows as active after a resume",
                    workspace.findViewById(R.id.surface_shading_faceted).isActivated());
            return null;
        });
    }

    @Test
    public void shd15_refusedIndexLeavesTheCurrentSettingStanding() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            // Set both explicitly rather than assuming what an earlier test
            // left behind: these settings are process-scoped native state and
            // the instrumented suites share one process.
            NativeViewport.setShadingModel(NativeViewport.SHADING_MATCAP);
            NativeViewport.setSurfaceShading(NativeViewport.SURFACE_SMOOTH);

            // An unknown index is a caller bug, not a value to repair: it is
            // refused and the current setting stands.
            assertEquals("an out-of-range shading model is refused",
                    NativeViewport.SHADING_MATCAP, NativeViewport.setShadingModel(99));
            assertEquals("a negative shading model is refused",
                    NativeViewport.SHADING_MATCAP, NativeViewport.setShadingModel(-1));
            assertEquals("an out-of-range surface shading is refused",
                    NativeViewport.SURFACE_SMOOTH, NativeViewport.setSurfaceShading(7));
            return null;
        });
    }
}
