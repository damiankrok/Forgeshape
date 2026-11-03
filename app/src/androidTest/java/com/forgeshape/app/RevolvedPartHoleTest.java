package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertEquals;
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
 * `CAD-REVOLVE-FACES-R1` on the device: a Construction Cone made a CAD part is
 * a revolved part; its flat base, the disc its base edge sweeps, offers a
 * Hole like any flat face, and Apply drills it as ONE undo step.
 *
 * <p>Results are asserted from native truth (the offer, the timeline, the
 * measured volume). Emulator evidence closes no physical-device gate.
 */
@RunWith(AndroidJUnit4.class)
public final class RevolvedPartHoleTest {

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    @Before
    public void startFromABox() {
        resetToBaselineConstruction(rule.getScenario());
    }

    @After
    public void restoreAProject() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.cadExtrudeCanvas().closeHoleEditor();
            NativeViewport.faceSketchOfferDismiss();
            NativeViewport.edgeFinishOfferDismiss();
            NativeViewport.enterConstructionMode();
            return null;
        });
        resetToBaselineConstruction(rule.getScenario());
    }

    @Test
    public void a_cone_parts_flat_base_takes_a_hole() {
        final int applied = NativeViewport.applyConstructionCone(2.0, 3.0);
        assertTrue("a cone: " + applied, applied == NativeViewport.APPLY_APPLIED
                || applied == NativeViewport.APPLY_UNCHANGED);
        final long cone = NativeViewport.sceneActiveBodyId();
        final int converted = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int status = NativeViewport.sceneConvertToCadPart(cone);
            workspace.onNativeStateChanged();
            return status;
        });
        settleLayout();
        assertEquals(NativeViewport.CAD_OK, converted);
        final long part = NativeViewport.sceneActiveBodyId();
        final double[] mass = new double[NativeViewport.MASS_SIZE];
        assertTrue(NativeViewport.bodyMassProperties(part, mass));
        final double before = mass[NativeViewport.MASS_VOLUME];

        // From below, onto the base disc at y = -1.5.
        setCamera(0.7f, -0.8f, 9.0f);
        realTap(worldToWindow(0.3, -1.5, 0.2));
        final double[] offer = new double[NativeViewport.HOLE_OFFER_SIZE];
        assertTrue("the revolved part's flat base offers a hole",
                NativeViewport.holeOfferState(offer));
        assertTrue("Hole is drawn", shown(R.id.cad_canvas_hole));
        realTap(centre(windowRect(R.id.cad_canvas_hole)));
        assertTrue("Hole opens the editor", shown(R.id.cad_canvas_hole_editor));
        final int undoBefore = NativeViewport.constructionUndoDepth();
        press(R.id.apply_hole);
        assertEquals("Apply is ONE undo step", undoBefore + 1, NativeViewport.constructionUndoDepth());
        assertEquals("the part carries one hole", 1, holeRows(part));
        assertTrue(NativeViewport.bodyMassProperties(part, mass));
        assertTrue("the hole removed material: " + before + " -> " + mass[NativeViewport.MASS_VOLUME],
                mass[NativeViewport.MASS_VOLUME] < before - 1e-4);
    }

    // -----------------------------------------------------------------------

    private static int holeRows(long body) {
        final double[] header = new double[NativeViewport.TIMELINE_HEADER_SIZE];
        final double[] rows =
                new double[NativeViewport.TIMELINE_MAX_ROWS * NativeViewport.TIMELINE_ROW_SIZE];
        final int count = NativeViewport.cadTimeline(body, false, header, rows);
        int holes = 0;
        for (int i = 0; i < count && i < NativeViewport.TIMELINE_MAX_ROWS; i++) {
            final int at = i * NativeViewport.TIMELINE_ROW_SIZE;
            if ((int) rows[at + NativeViewport.TIMELINE_ROW_KIND]
                    == NativeViewport.TIMELINE_KIND_HOLE) {
                holes++;
            }
        }
        return holes;
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
