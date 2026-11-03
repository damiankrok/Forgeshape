package com.forgeshape.app;

import static com.forgeshape.app.SketchTestSupport.drawRectangleAndExtrude;
import static com.forgeshape.app.SketchTestSupport.sketchState;
import static com.forgeshape.app.SketchTestSupport.sketchViewState;
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
 * `STARTUP-FACE-ICONS-R1` on the device: after an extrusion, an ordinary tap on
 * a planar face of the body offers "Sketch here" beside the finger, and one tap
 * on that chip opens a sketch ON THAT FACE -- no drawer, no chooser, no second
 * aim. A tap on empty space withdraws the offer, and nothing about the project
 * moves until the sketch is committed.
 *
 * <p>Every touch is a real window touch through the decor view. Asserted from
 * native truth (sketch state, face support, the projected sketch origin).
 * Emulator evidence closes no physical-device gate.
 */
@RunWith(AndroidJUnit4.class)
public final class FaceSketchOfferTest {

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
            NativeViewport.enterConstructionMode();
            workspace.showHomeAsFirstLaunchForTest();
            return null;
        });
        settleLayout();
    }

    @After
    public void restoreAProject() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.supportChooserCancel();
            NativeViewport.sketchCancel();
            NativeViewport.faceSketchOfferDismiss();
            NativeViewport.enterConstructionMode();
            return null;
        });
        resetToBaselineConstruction(rule.getScenario());
    }

    @Test
    public void a_tap_on_a_planar_face_offers_a_sketch_there_and_one_more_tap_opens_it() {
        newCadProjectBlock();
        setCamera(0.7f, 0.9f, 8.0f);
        final int undoBefore = NativeViewport.constructionUndoDepth();
        assertFalse("nothing is offered before a face is tapped",
                shown(R.id.cad_canvas_sketch_on_face));

        // The far cap of a 2 x 2 x 1 block is at z = 1.
        realTap(worldToWindow(0.5, 0.5, 1.0));
        assertTrue("the tap on the cap offers Sketch here", shown(R.id.cad_canvas_sketch_on_face));
        assertEquals("an offer is not a sketch", NativeViewport.SKETCH_INACTIVE, sketchState());
        assertEquals("an offer records nothing", undoBefore, NativeViewport.constructionUndoDepth());

        // Empty space withdraws it.
        final float[] cap = worldToWindow(0.5, 0.5, 1.0);
        final float viewportWidth = onWorkspace(rule.getScenario(), (activity, workspace) ->
                (float) workspace.findViewById(R.id.viewport_surface).getWidth());
        realTap(new float[]{cap[0] - 0.3f * viewportWidth, cap[1]});
        assertFalse("a tap off the body withdraws the offer",
                shown(R.id.cad_canvas_sketch_on_face));

        // Offer again, then take it.
        realTap(worldToWindow(0.5, 0.5, 1.0));
        assertTrue(shown(R.id.cad_canvas_sketch_on_face));
        realTap(centre(windowRect(R.id.cad_canvas_sketch_on_face)));
        assertEquals("one tap on the chip opens the sketch", NativeViewport.SKETCH_EDITING,
                sketchState());
        assertEquals("on the face", 1.0,
                sketchViewState()[NativeViewport.SKETCH_VIEW_FACE_SUPPORTED], 0.0);
        final float[] origin = new float[2];
        final float[] world = new float[2];
        assertTrue(NativeViewport.sketchScreenPoint(0.0, 0.0, origin));
        assertTrue(NativeViewport.debugProjectWorld(0.0, 0.0, 1.0, world));
        assertEquals("the sketch stands on the tapped cap (x)", world[0], origin[0], 1.5f);
        assertEquals("the sketch stands on the tapped cap (y)", world[1], origin[1], 1.5f);
        assertFalse("the chip is gone while sketching", shown(R.id.cad_canvas_sketch_on_face));
        assertEquals("opening a sketch records nothing", undoBefore, NativeViewport.constructionUndoDepth());
    }

    // -----------------------------------------------------------------------

    private void newCadProjectBlock() {
        press(R.id.home_new_project);
        press(R.id.new_project_cad);
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
        final long body = drawRectangleAndExtrude(rule.getScenario(), 2.0, 2.0, "1");
        assertTrue(body > 0);
        assertTrue(NativeViewport.projectOpen());
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
