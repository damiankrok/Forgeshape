package com.forgeshape.app;

import static com.forgeshape.app.SketchTestSupport.drawRectangleAndExtrude;
import static com.forgeshape.app.SketchTestSupport.sketchState;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.graphics.Rect;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * `CAD-MEASURE-R1` on the device: the CAD part's Measure section turns
 * Measure distance on; two real taps beside two corners of the top of a
 * 2 x 2 x 1 block snap to those corners and read exactly 2 m, the panel shows
 * the distance, and Stop measuring gives the model's taps back.
 *
 * <p>Results are asserted from native truth (the measure state). Emulator
 * evidence closes no physical-device gate.
 */
@RunWith(AndroidJUnit4.class)
public final class MeasureDistanceTest {

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    @Before
    public void startAtHome() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.dismissRecoveryPromptForTest();
            NativeViewport.supportChooserCancel();
            NativeViewport.sketchCancel();
            NativeViewport.faceSketchOfferDismiss();
            NativeViewport.edgeFinishOfferDismiss();
            NativeViewport.measureSetActive(false);
            NativeViewport.enterConstructionMode();
            workspace.showHomeAsFirstLaunchForTest();
            return null;
        });
        settleLayout();
    }

    @After
    public void restoreAProject() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.measureSetActive(false);
            NativeViewport.supportChooserCancel();
            NativeViewport.sketchCancel();
            NativeViewport.faceSketchOfferDismiss();
            NativeViewport.edgeFinishOfferDismiss();
            NativeViewport.enterConstructionMode();
            return null;
        });
        resetToBaselineConstruction(rule.getScenario());
    }

    @Test
    public void two_taps_beside_two_corners_measure_the_edge_between_them() {
        block();
        setCamera(0.7f, 0.9f, 8.0f);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onNativeStateChanged();
            WorkspaceTestSupport.openPrecision(workspace);
            return null;
        });
        settleLayout();
        assertTrue("the part's Measure section offers Measure distance",
                shown(R.id.measure_distance));
        press(R.id.measure_distance);
        assertTrue("Measure owns the taps", NativeViewport.measureActive());
        final long active = NativeViewport.sceneActiveBodyId();

        realTap(worldToWindow(-0.97, -0.97, 1.0));
        realTap(worldToWindow(0.97, -0.97, 1.0));
        final double[] measure = new double[NativeViewport.MEASURE_SIZE];
        assertTrue(NativeViewport.measureState(measure));
        assertEquals("both points picked", 2, (int) measure[NativeViewport.MEASURE_COUNT]);
        assertEquals("snapped to the two corners, the edge is exactly 2 m", 2.0,
                measure[NativeViewport.MEASURE_DISTANCE], 1e-4);
        assertEquals("along X", 2.0, Math.abs(measure[NativeViewport.MEASURE_DX]), 1e-4);
        assertEquals("the active body did not move", active, NativeViewport.sceneActiveBodyId());
        final String shown = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final android.widget.TextView text = activity.findViewById(R.id.measure_distance_result);
            return text == null ? "" : text.getText().toString();
        });
        assertTrue("the panel reads the distance: " + shown, shown.contains("2"));

        press(R.id.measure_distance);
        assertFalse("Stop measuring gives the taps back", NativeViewport.measureActive());
        assertFalse(NativeViewport.measureState(measure));
    }

    // -----------------------------------------------------------------------

    /** A 2 x 2 x 1 CAD block. */
    private long block() {
        press(R.id.home_new_project);
        press(R.id.new_project_cad);
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
        final long body = drawRectangleAndExtrude(rule.getScenario(), 2.0, 2.0, "1");
        assertTrue(body > 0);
        return body;
    }

    private float[] worldToWindow(double x, double y, double z) {
        final float[] at = new float[2];
        assertTrue("the point projects", NativeViewport.debugProjectWorld(x, y, z, at));
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final View root = activity.getWindow().getDecorView();
            final int[] vp = new int[2];
            final int[] rp = new int[2];
            viewport.getLocationInWindow(vp);
            root.getLocationInWindow(rp);
            return new float[]{at[0] + vp[0] - rp[0], at[1] + vp[1] - rp[1]};
        });
    }

    private void realTap(final float[] window) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View root = activity.getWindow().getDecorView();
            final long down = SystemClock.uptimeMillis();
            dispatch(root, down, down, MotionEvent.ACTION_DOWN, window[0], window[1]);
            dispatch(root, down, down + 40L, MotionEvent.ACTION_UP, window[0], window[1]);
            return null;
        });
        settleLayout();
    }

    private static void dispatch(View root, long downTime, long eventTime, int action, float x,
                                 float y) {
        final MotionEvent event = MotionEvent.obtain(downTime, eventTime, action, x, y, 0);
        event.setSource(InputDevice.SOURCE_TOUCHSCREEN);
        root.dispatchTouchEvent(event);
        event.recycle();
    }

    private Rect windowRect(int id) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View view = activity.findViewById(id);
            assertNotNull(view);
            final View root = activity.getWindow().getDecorView();
            final int[] at = new int[2];
            final int[] rp = new int[2];
            view.getLocationInWindow(at);
            root.getLocationInWindow(rp);
            return new Rect(at[0] - rp[0], at[1] - rp[1], at[0] - rp[0] + view.getWidth(),
                    at[1] - rp[1] + view.getHeight());
        });
    }

    private static float[] centre(Rect r) {
        return new float[]{r.exactCenterX(), r.exactCenterY()};
    }

    private void press(final int id) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View control = workspace.findViewById(id);
            assertNotNull(control);
            assertTrue(control.isShown());
            control.performClick();
            return null;
        });
        settleLayout();
    }

    private boolean shown(int id) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View view = activity.findViewById(id);
            return view != null && view.isShown();
        });
    }

    private void setCamera(float yaw, float pitch, float distance) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(NativeViewport.debugSetCameraPose(yaw, pitch, distance));
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }
}
