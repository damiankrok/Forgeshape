package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.nativeSnapshot;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.os.ParcelFileDescriptor;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.FileInputStream;
import java.io.InputStream;

/**
 * R1C1-10..14 and R1C1-21..24 - the motion language, on a device.
 *
 * <p><b>Every case asserts a RESTING state, never a frame of an animation.</b>
 * A test that sampled a transition part-way through would be a test of the
 * device's frame timing, and would fail on a slow emulator for reasons that
 * have nothing to do with the product. What matters, and what is checked here,
 * is that the panel always ends up somewhere legitimate: fully visible or fully
 * gone, at alpha 1, untranslated, with a chevron that agrees with it - however
 * the user interrupted it on the way.
 *
 * <p>No control is located by coordinate and no rendered pixel is asserted, as
 * everywhere else in this suite.
 */
@RunWith(AndroidJUnit4.class)
public class EditorWorkspaceMotionTest {

    /** What the animator scale is put back to. 1.0 is the platform default and
     *  what every other suite in this project assumes. */
    private static final String NORMAL_SCALE = "1.0";

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    @Before
    public void setUp() {
        // Also a REPAIR: a case that died half-way through could otherwise leave
        // the device with animation switched off for every suite that runs
        // after it, and the failure would look like a motion defect.
        setAnimatorScale(NORMAL_SCALE);
        resetToBaselineConstruction(rule.getScenario());
    }

    @After
    public void tearDown() {
        setAnimatorScale(NORMAL_SCALE);
    }

    // -----------------------------------------------------------------------
    // R1C1-10 -- the Display popover's behaviour is preserved
    // -----------------------------------------------------------------------

    /**
     * The popover moved onto the shared helper at UI-R1C1, so what has to be
     * re-proved is that it still does exactly what it did: opens, closes, and
     * obeys reduced motion by landing rather than by hurrying.
     */
    @Test
    public void r1c110_theDisplayPopoverStillOpensClosesAndObeysReducedMotion() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View popover = workspace.findViewById(R.id.display_settings_popover);
            assertEquals("the popover starts closed", View.GONE, popover.getVisibility());
            workspace.findViewById(R.id.display_settings_button).performClick();
            assertEquals("tapping Display opens it", View.VISIBLE, popover.getVisibility());
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View popover = workspace.findViewById(R.id.display_settings_popover);
            assertEquals("and it settles fully open", 1.0f, popover.getAlpha(), 1.0e-6f);
            assertEquals(1.0f, popover.getScaleX(), 1.0e-6f);
            workspace.findViewById(R.id.display_settings_button).performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("tapping it again closes it", View.GONE,
                    workspace.findViewById(R.id.display_settings_popover).getVisibility());
            return null;
        });

        setAnimatorScale("0");
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View popover = workspace.findViewById(R.id.display_settings_popover);
            workspace.findViewById(R.id.display_settings_button).performClick();
            // No settle: reduced motion means the final state is already true on
            // the very next line, which is the whole difference between landing
            // and running a shortened animation.
            assertEquals("reduced motion opens it at once", View.VISIBLE,
                    popover.getVisibility());
            assertEquals(1.0f, popover.getAlpha(), 1.0e-6f);
            assertEquals(1.0f, popover.getScaleX(), 1.0e-6f);
            workspace.findViewById(R.id.display_settings_button).performClick();
            assertEquals("and closes it at once", View.GONE, popover.getVisibility());
            return null;
        });
    }

    /** In-place selection is what makes comparing options feel like one act;
     *  choosing a mode must not move, re-animate or close the panel. */
    @Test
    public void r1c110_choosingAnOptionLeavesThePopoverExactlyWhereItIs() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.display_settings_button).performClick();
            return null;
        });
        settleLayout();
        final int[] before = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View popover = workspace.findViewById(R.id.display_settings_popover);
            return new int[]{popover.getLeft(), popover.getTop(),
                    popover.getWidth(), popover.getHeight()};
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.surface_shading_faceted).performClick();
            final View popover = workspace.findViewById(R.id.display_settings_popover);
            assertEquals("the panel stays open", View.VISIBLE, popover.getVisibility());
            assertEquals("and does not re-animate", 1.0f, popover.getAlpha(), 1.0e-6f);
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View popover = workspace.findViewById(R.id.display_settings_popover);
            assertArrayEquals("and does not move", before, new int[]{popover.getLeft(),
                    popover.getTop(), popover.getWidth(), popover.getHeight()});
            workspace.findViewById(R.id.surface_shading_smooth).performClick();
            workspace.findViewById(R.id.display_settings_button).performClick();
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // R1C1-11 / R1C1-21 -- the inspector always settles somewhere legitimate
    // -----------------------------------------------------------------------

    @Test
    public void r1c111_theInspectorEndsAtTheSameRestingLayoutEitherWay() {
        for (int round = 0; round < 2; round++) {
            final boolean expanded = onWorkspace(rule.getScenario(), (activity, workspace) ->
                    workspace.propertyInspector().isExpanded());
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                workspace.findViewById(R.id.inspector_toggle).performClick();
                return null;
            });
            settleLayout();
            assertInspectorSettled("after toggling from expanded=" + expanded, !expanded);
        }
    }

    /**
     * R1C1-12 / R1C1-21. A rapid toggle must reverse, not queue: every
     * transition cancels whatever was running on the same view first. Without
     * that, the panel settles on whichever animation happened to finish last
     * and can be left half transparent with no gesture left to fix it.
     */
    @Test
    public void r1c112_rapidTogglingCannotLeaveTheInspectorHalfVisible() {
        final boolean startExpanded = onWorkspace(rule.getScenario(), (activity, workspace) ->
                workspace.propertyInspector().isExpanded());

        // Six toggles with no settle between them, so several land squarely in
        // the middle of the transition before them.
        for (int i = 0; i < 6; i++) {
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                workspace.findViewById(R.id.inspector_toggle).performClick();
                return null;
            });
            SystemClock.sleep(30L);
        }
        settleLayout();
        // Six toggles is an even number, so the detent is back where it began.
        assertInspectorSettled("after six rapid toggles", startExpanded);
    }

    private void assertInspectorSettled(final String where, final boolean expanded) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final PropertyInspectorView inspector = workspace.propertyInspector();
            final View scroll = workspace.findViewById(R.id.inspector_scroll);
            assertEquals(where + ": the detent is what was asked for",
                    expanded, inspector.isExpanded());
            assertEquals(where + ": the body is fully shown or fully gone",
                    expanded ? View.VISIBLE : View.GONE, scroll.getVisibility());
            assertEquals(where + ": nothing is left part-way faded",
                    1.0f, scroll.getAlpha(), 1.0e-6f);
            assertEquals(where + ": nothing is left displaced",
                    0.0f, scroll.getTranslationY(), 1.0e-6f);
            // The chevron points the way the panel will go. An interrupted
            // transition must never leave it claiming the opposite.
            assertEquals(where + ": the chevron agrees with the detent",
                    activity.getString(expanded ? R.string.inspector_collapse
                            : R.string.inspector_expand),
                    workspace.findViewById(R.id.inspector_toggle).getContentDescription());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // R1C1-13 / R1C1-23 -- chrome hide/restore is alpha only
    // -----------------------------------------------------------------------

    @Test
    public void r1c113_chromeHideAndRestoreEndOnTheCorrectVisibility() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.hide_ui_toggle).performClick();
            assertTrue(workspace.chromeHidden());
            assertEquals("the way back is available at once", View.VISIBLE,
                    workspace.findViewById(R.id.restore_ui_chip).getVisibility());
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("the chrome is gone, not merely transparent", View.GONE,
                    ((View) workspace.findViewById(R.id.global_toolbar).getParent())
                            .getVisibility());
            assertEquals("and stands between the user and the model nowhere",
                    0, workspace.chromeRects().length);
            assertEquals(1.0f,
                    workspace.findViewById(R.id.restore_ui_chip).getAlpha(), 1.0e-6f);
            workspace.findViewById(R.id.restore_ui_chip).performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertFalse(workspace.chromeHidden());
            assertEquals("restoring brings the toolbar all the way back", View.VISIBLE,
                    workspace.findViewById(R.id.global_toolbar).getVisibility());
            assertEquals(1.0f,
                    ((View) workspace.findViewById(R.id.global_toolbar).getParent())
                            .getAlpha(), 1.0e-6f);
            assertEquals("and puts the affordance away", View.GONE,
                    workspace.findViewById(R.id.restore_ui_chip).getVisibility());
            return null;
        });
    }

    /**
     * R1C1-23. The Vulkan viewport is full-bleed, so a chrome transition has no
     * size to change - and one that DID change a size would rebuild the
     * swapchain for a question about where buttons are drawn.
     */
    @Test
    public void r1c123_hidingAndRestoringChromeNeverResizesTheViewport() {
        final int[] before = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.getChildAt(0);
            return new int[]{viewport.getWidth(), viewport.getHeight()};
        });
        final double[] nativeBefore = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.hide_ui_toggle).performClick();
            return null;
        });
        settleLayout();
        final int[] hidden = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.getChildAt(0);
            return new int[]{viewport.getWidth(), viewport.getHeight()};
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.restore_ui_chip).performClick();
            return null;
        });
        settleLayout();
        final int[] after = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.getChildAt(0);
            return new int[]{viewport.getWidth(), viewport.getHeight()};
        });

        assertArrayEquals("hiding chrome must not resize the surface", before, hidden);
        assertArrayEquals("and neither must restoring it", before, after);
        assertArrayEquals("and neither may touch anything below JNI",
                nativeBefore, onWorkspace(rule.getScenario(),
                        (activity, workspace) -> nativeSnapshot()), 0.0);
    }

    // -----------------------------------------------------------------------
    // R1C1-14 / R1C1-24 -- reduced motion
    // -----------------------------------------------------------------------

    @Test
    public void r1c114_reducedMotionMakesChromeAndTheInspectorInstant() {
        setAnimatorScale("0");

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View scroll = workspace.findViewById(R.id.inspector_scroll);
            final boolean expanded = workspace.propertyInspector().isExpanded();
            workspace.findViewById(R.id.inspector_toggle).performClick();
            // Asserted on the very next line, with no settle at all: reduced
            // motion means the final state is already true.
            assertEquals("the detent lands at once", !expanded,
                    workspace.propertyInspector().isExpanded());
            assertEquals(!expanded ? View.VISIBLE : View.GONE, scroll.getVisibility());
            assertEquals(1.0f, scroll.getAlpha(), 1.0e-6f);
            assertEquals(0.0f, scroll.getTranslationY(), 1.0e-6f);

            workspace.findViewById(R.id.hide_ui_toggle).performClick();
            assertEquals("chrome hides at once", View.VISIBLE,
                    workspace.findViewById(R.id.restore_ui_chip).getVisibility());
            workspace.findViewById(R.id.restore_ui_chip).performClick();
            assertEquals("and comes back at once", View.VISIBLE,
                    workspace.findViewById(R.id.global_toolbar).getVisibility());
            assertEquals(1.0f,
                    ((View) workspace.findViewById(R.id.global_toolbar).getParent())
                            .getAlpha(), 1.0e-6f);
            return null;
        });
    }

    /**
     * R1C1-24. The same signal has to reach the viewport, because the selection
     * acknowledgement is drawn there rather than by a Java animator. What
     * crosses is one bool: no Android type, and nothing about a scale.
     */
    @Test
    public void r1c124_theViewportIsToldAboutReducedMotionToo() {
        setAnimatorScale("0");
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.syncFromNative();
            assertTrue("the viewport must know motion is reduced",
                    NativeViewport.reducedMotion());
            return null;
        });
        setAnimatorScale(NORMAL_SCALE);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.syncFromNative();
            assertFalse("and must know when it is not",
                    NativeViewport.reducedMotion());
            return null;
        });
    }

    /** Reduced motion is presentation: it may not disturb anything below JNI. */
    @Test
    public void r1c124_reducedMotionMintsNoRevisionAndChangesNoState() {
        final double[] before = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());
        setAnimatorScale("0");
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.syncFromNative();
            return null;
        });
        setAnimatorScale(NORMAL_SCALE);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.syncFromNative();
            return null;
        });
        final double[] after = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());
        assertArrayEquals("reduced motion is timing and nothing else:"
                + WorkspaceTestSupport.describeSnapshotDifference(before, after),
                before, after, 0.0);
    }

    // -----------------------------------------------------------------------
    // R1C1-22 -- a viewport gesture outranks chrome motion
    // -----------------------------------------------------------------------

    /**
     * While a pointer is down on the model - in Sculpt Mode, a real stroke -
     * chrome must not spend main-thread time on a transition. A second finger
     * reaching the inspector header is exactly the case: the panel snaps
     * instead of animating, and the gesture's own arbitration is untouched.
     */
    @Test
    public void r1c122_aChromeTransitionDuringAViewportGestureIsInstant() {
        final double[] before = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.getChildAt(0);
            final long down = SystemClock.uptimeMillis();
            sendViewportTouch(viewport, down, down, MotionEvent.ACTION_DOWN);

            final View scroll = workspace.findViewById(R.id.inspector_scroll);
            final boolean expanded = workspace.propertyInspector().isExpanded();
            workspace.findViewById(R.id.inspector_toggle).performClick();
            // No settle: with a pointer on the model the panel must already be
            // at its resting state rather than part-way through a fade.
            assertEquals("the detent lands at once during a gesture", !expanded,
                    workspace.propertyInspector().isExpanded());
            assertEquals(!expanded ? View.VISIBLE : View.GONE, scroll.getVisibility());
            assertEquals(1.0f, scroll.getAlpha(), 1.0e-6f);

            sendViewportTouch(viewport, down, down + 32L, MotionEvent.ACTION_UP);
            return null;
        });
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            // And once the gesture has settled, motion is available again.
            final View scroll = workspace.findViewById(R.id.inspector_scroll);
            assertEquals(1.0f, scroll.getAlpha(), 1.0e-6f);
            assertEquals(0.0f, scroll.getTranslationY(), 1.0e-6f);
            return null;
        });

        final double[] after = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());
        assertArrayEquals("a chrome detent change is UI and nothing else:"
                + WorkspaceTestSupport.describeSnapshotDifference(before, after),
                before, after, 0.0);
    }

    private static void sendViewportTouch(View viewport, long downTime, long eventTime,
                                          int action) {
        final MotionEvent event = MotionEvent.obtain(downTime, eventTime, action,
                viewport.getWidth() * 0.5f, viewport.getHeight() * 0.5f, 0);
        try {
            viewport.dispatchTouchEvent(event);
        } finally {
            event.recycle();
        }
    }

    // -----------------------------------------------------------------------
    // The animator scale
    // -----------------------------------------------------------------------

    /**
     * Sets the platform's animator duration scale for the duration of a case.
     *
     * <p>Written through the instrumentation's own shell, because the product
     * holds no {@code WRITE_SECURE_SETTINGS} permission and must never ask for
     * one: reduced motion is something the platform tells ForgeShape, not
     * something ForgeShape may decide. Restored in both {@code @Before} and
     * {@code @After}, so a case that dies part-way through cannot leave the
     * device with animation switched off for every suite after it.
     */
    private static void setAnimatorScale(String scale) {
        shell("settings put global animator_duration_scale " + scale);
        // The scale is read per use, not cached, so nothing has to be told.
        SystemClock.sleep(50L);
    }

    private static void shell(String command) {
        final ParcelFileDescriptor pfd = InstrumentationRegistry.getInstrumentation()
                .getUiAutomation().executeShellCommand(command);
        try (InputStream in = new FileInputStream(pfd.getFileDescriptor())) {
            final byte[] buffer = new byte[256];
            while (in.read(buffer) > 0) {
                // Drained so the command completes rather than blocking on a
                // full pipe; its output is not interesting.
            }
        } catch (Exception ignored) {
            // A shell failure means the scale did not change; the assertions
            // that depend on it will say so far more clearly than a rethrow
            // from a helper would.
        }
    }
}
