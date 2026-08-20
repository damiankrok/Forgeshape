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
            return null;
        });
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
