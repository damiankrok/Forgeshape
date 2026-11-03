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
 * `CAD-UI-DOCK-R1` on the device: after a tap on a CAD body's face, the face's
 * acts (Sketch here, Round / Bevel edges, Hole) and the feature editor they
 * open stand DOCKED beside the trailing controls -- never on those controls and
 * never on the point that was tapped -- a marker alone stays at that point, and
 * a drag starting on the model still orbits the camera while an editor is
 * open.
 *
 * <p>Positions are read from the views' own window rectangles and native's
 * projection; nothing locates a control by a fixed coordinate. Emulator
 * evidence closes no physical-device gate.
 */
@RunWith(AndroidJUnit4.class)
public final class CadDockTest {

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
            NativeViewport.enterConstructionMode();
            workspace.showHomeAsFirstLaunchForTest();
            return null;
        });
        settleLayout();
    }

    @After
    public void restoreAProject() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.cadExtrudeCanvas().closeHoleEditor();
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
    public void face_acts_and_editors_dock_beside_the_trailing_controls() {
        press(R.id.home_new_project);
        press(R.id.new_project_cad);
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
        assertTrue(drawRectangleAndExtrude(rule.getScenario(), 2.0, 2.0, "1") > 0);
        setCamera(0.7f, 0.9f, 8.0f);
        final float[] tapped = worldToWindow(0.3, 0.3, 1.0);
        realTap(tapped);

        final Rect host = windowRect(R.id.workspace_trailing_host);
        for (int id : new int[] {R.id.cad_canvas_sketch_on_face, R.id.cad_canvas_edge_finish_bar}) {
            assertTrue("offered: " + id, shown(id));
            final Rect docked = windowRect(id);
            assertFalse("a docked act never stands on the trailing controls",
                    Rect.intersects(docked, host));
            assertTrue("and stands on their inner side", docked.right <= host.left + 1
                    || docked.left >= host.right - 1);
            assertFalse("and never on the point the finger tapped",
                    docked.contains(Math.round(tapped[0]), Math.round(tapped[1])));
        }
        assertTrue("a marker stays at the tapped point", onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.cadExtrudeCanvas().offerMarkerShown()));
        final Rect sketchHere = windowRect(R.id.cad_canvas_sketch_on_face);
        final Rect bar = windowRect(R.id.cad_canvas_edge_finish_bar);
        assertFalse("the face's acts do not stand on each other", Rect.intersects(sketchHere, bar));

        realTap(centre(windowRect(R.id.cad_canvas_hole)));
        assertTrue("Hole opens its editor", shown(R.id.cad_canvas_hole_editor));
        final Rect editor = windowRect(R.id.cad_canvas_hole_editor);
        assertFalse("the editor never stands on the trailing controls",
                Rect.intersects(editor, host));
        assertFalse("nor on the point the hole will be drilled",
                editor.contains(Math.round(tapped[0]), Math.round(tapped[1])));

        // The model stays a place to navigate: a drag from the tapped point,
        // away from the dock, orbits.
        final float[] before = new float[3];
        NativeViewport.debugCameraPose(before);
        final boolean towardLeft = editor.left > tapped[0];
        realDrag(tapped, new float[] {tapped[0] + (towardLeft ? -220f : 220f), tapped[1] + 40f});
        final float[] after = new float[3];
        NativeViewport.debugCameraPose(after);
        assertTrue("a drag on the model still orbits while the editor is open",
                Math.abs(after[0] - before[0]) > 0.05f);
        assertTrue("and the editor stays open", shown(R.id.cad_canvas_hole_editor));
    }

    // -----------------------------------------------------------------------

    private void realDrag(final float[] from, final float[] to) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View root = activity.getWindow().getDecorView();
            final long down = SystemClock.uptimeMillis();
            dispatch(root, down, down, MotionEvent.ACTION_DOWN, from[0], from[1]);
            for (int k = 1; k <= 10; k++) {
                final float t = k / 10f;
                dispatch(root, down, down + 16L * k, MotionEvent.ACTION_MOVE,
                        from[0] + (to[0] - from[0]) * t, from[1] + (to[1] - from[1]) * t);
            }
            dispatch(root, down, down + 200L, MotionEvent.ACTION_UP, to[0], to[1]);
            return null;
        });
        settleLayout();
    }

    private boolean activated(int id) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View view = activity.findViewById(id);
            return view != null && view.isShown() && view.isActivated();
        });
    }

    private String text(int id) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final android.widget.TextView view = activity.findViewById(id);
            return view == null ? "" : view.getText().toString();
        });
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
