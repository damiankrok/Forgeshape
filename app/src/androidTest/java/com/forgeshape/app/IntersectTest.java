package com.forgeshape.app;

import static com.forgeshape.app.SketchTestSupport.dragSketch;
import static com.forgeshape.app.SketchTestSupport.drawRectangleAndExtrude;
import static com.forgeshape.app.SketchTestSupport.selectTool;
import static com.forgeshape.app.SketchTestSupport.sketchEntityCount;
import static com.forgeshape.app.SketchTestSupport.sketchState;
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
 * `CAD-INTERSECT-R1` on the device: a circle sketched on the top of a CAD
 * block offers Intersect beside Add and Cut; choosing it in the precision
 * surface grows the extrusion INTO the block, and the commit keeps only the
 * disc the extrusion passes through, as ONE undo step on the same body.
 *
 * <p>Results are asserted from native truth (the operation, the measured
 * volume, the undo depth). Emulator evidence closes no physical-device gate.
 */
@RunWith(AndroidJUnit4.class)
public final class IntersectTest {

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
    public void a_face_sketch_intersects_the_block_down_to_the_disc_it_passes_through() {
        final long body = block();
        sketchOnTop(0.3, 0.3);
        selectTool(rule.getScenario(), R.id.tool_rail_circle);
        dragSketch(rule.getScenario(), 0.0, 0.0, 0.5, 0.0);
        assertEquals("the circle was drawn", 1, sketchEntityCount());
        press(R.id.finish_sketch);
        assertEquals(NativeViewport.SKETCH_READY, sketchState());
        final double[] tool = new double[NativeViewport.CAD_EXTRUDE_SIZE];
        NativeViewport.cadExtrudeToolState(tool);
        assertTrue("Intersect is offered on a part's face",
                ((int) tool[NativeViewport.CAD_EXTRUDE_OPERATIONS_AVAILABLE]
                        & NativeViewport.OPERATION_BIT_INTERSECT) != 0);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onNativeStateChanged();
            WorkspaceTestSupport.openPrecision(workspace);
            return null;
        });
        settleLayout();
        assertTrue("the precision surface lists it", shown(R.id.sketch_operation_intersect));
        press(R.id.sketch_operation_intersect);
        NativeViewport.cadExtrudeToolState(tool);
        assertEquals("the operation is Intersect", NativeViewport.OPERATION_INTERSECT,
                (int) tool[NativeViewport.CAD_EXTRUDE_OPERATION]);
        assertEquals(NativeViewport.CAD_OK, NativeViewport.sketchSetExtrude(0.5,
                (int) tool[NativeViewport.CAD_EXTRUDE_DIRECTION]));
        NativeViewport.cadExtrudeToolState(tool);
        assertEquals("the intersect would commit", NativeViewport.CAD_OK,
                (int) tool[NativeViewport.CAD_EXTRUDE_CANDIDATE_STATUS]);
        final int undoBefore = NativeViewport.constructionUndoDepth();
        commitFromPrecisionSurface();
        assertEquals("committed", NativeViewport.SKETCH_INACTIVE, sketchState());
        assertEquals("as ONE undo step", undoBefore + 1, NativeViewport.constructionUndoDepth());
        final double[] mass = new double[NativeViewport.MASS_SIZE];
        assertTrue(NativeViewport.bodyMassProperties(body, mass));
        // The kept piece is the disc under the circle, 0.5 deep: as tall as the
        // extrusion, as wide as the circle, and far less than the 4 m³ block.
        assertEquals("as deep as the extrusion", 0.5, mass[NativeViewport.MASS_SIZE_X + 2], 1e-4);
        assertTrue("as wide as the circle", mass[NativeViewport.MASS_SIZE_X] < 1.3
                && mass[NativeViewport.MASS_SIZE_X] > 0.7);
        // The drawn circle's own radius is what the finger made it; the kept
        // piece is the disc of that radius, 0.5 deep.
        final double radius = 0.5 * mass[NativeViewport.MASS_SIZE_X];
        assertEquals("a disc of the circle's volume", Math.PI * radius * radius * 0.5,
                mass[NativeViewport.MASS_VOLUME], 0.05 * Math.PI * radius * radius * 0.5);
        assertTrue("far less than the 4 m³ block", mass[NativeViewport.MASS_VOLUME] < 1.5);
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

    /** Taps the block's top at (x, y) and takes Sketch here. */
    private void sketchOnTop(double x, double y) {
        setCamera(0.7f, 0.9f, 8.0f);
        realTap(worldToWindow(x, y, 1.0));
        assertTrue("the top face offers a sketch", shown(R.id.cad_canvas_sketch_on_face));
        press(R.id.cad_canvas_sketch_on_face);
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
    }

    private void commitFromPrecisionSurface() {
        refresh();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.sketch_extrude_commit).performClick();
            return null;
        });
        settleLayout();
    }

    private void refresh() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
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
