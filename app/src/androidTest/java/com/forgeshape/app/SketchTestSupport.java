package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;
import android.widget.EditText;

import androidx.test.core.app.ActivityScenario;

/**
 * The sketch and support-chooser gestures the device suites share.
 *
 * <p>Every pixel here is asked for from native code — {@code sketchScreenPoint}
 * for a sketch coordinate, {@code debugProjectWorld} for a world point — through
 * the same projection native unprojects with, and never written down. Every
 * gesture goes through the real {@code SurfaceView}, so it takes the production
 * path: the surface converts the {@code MotionEvent}, native arbitrates it, and
 * the workspace learns the gesture settled exactly as it does for a user.
 */
final class SketchTestSupport {

    private SketchTestSupport() {
    }

    /** A double-tap at the pixel a world point projects to: aim, then commit. */
    static void tapTapWorld(ActivityScenario<ForgeShapeActivity> scenario, double x, double y,
                            double z) {
        final float[] at = new float[2];
        assertTrue("the target projects on screen",
                NativeViewport.debugProjectWorld(x, y, z, at));
        tapViewport(scenario, at[0], at[1]);
        tapViewport(scenario, at[0], at[1]);
    }

    /** One tap at the pixel a world point projects to: aim only. */
    static void tapWorld(ActivityScenario<ForgeShapeActivity> scenario, double x, double y,
                         double z) {
        final float[] at = new float[2];
        assertTrue("the target projects on screen",
                NativeViewport.debugProjectWorld(x, y, z, at));
        tapViewport(scenario, at[0], at[1]);
    }

    /** Drives the Tool Rail entry a user would press. */
    static void selectTool(ActivityScenario<ForgeShapeActivity> scenario, final int toolRailId) {
        doOnWorkspace(scenario, (activity, workspace) -> {
            workspace.findViewById(toolRailId).performClick();
            return null;
        });
        settleLayout();
    }

    /** Finish Sketch, type a depth, Extrude. Asserts each transition. */
    static void finishAndExtrude(ActivityScenario<ForgeShapeActivity> scenario,
                                 final String depth) {
        doOnWorkspace(scenario, (activity, workspace) -> {
            workspace.findViewById(R.id.finish_sketch).performClick();
            return null;
        });
        settleLayout();
        assertEquals(NativeViewport.SKETCH_READY, sketchState());
        doOnWorkspace(scenario, (activity, workspace) -> {
            final EditText field = workspace.sketchEditor().findViewById(R.id.field_extrude_depth);
            field.setText(depth);
            workspace.findViewById(R.id.sketch_extrude_commit).performClick();
            return null;
        });
        settleLayout();
        assertEquals(NativeViewport.SKETCH_INACTIVE, sketchState());
    }

    /** A drag between two sketch points, through real Down/Move/Up. */
    static void dragSketch(ActivityScenario<ForgeShapeActivity> scenario, double u0, double v0,
                           double u1, double v1) {
        final float[] from = new float[2];
        final float[] to = new float[2];
        assertTrue(NativeViewport.sketchScreenPoint(u0, v0, from));
        assertTrue(NativeViewport.sketchScreenPoint(u1, v1, to));
        doOnWorkspace(scenario, (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final long down = SystemClock.uptimeMillis();
            send(viewport, down, down, MotionEvent.ACTION_DOWN, from[0], from[1]);
            for (int step = 1; step <= 6; ++step) {
                final float t = step / 6f;
                send(viewport, down, down + step * 12L, MotionEvent.ACTION_MOVE,
                        from[0] + (to[0] - from[0]) * t, from[1] + (to[1] - from[1]) * t);
            }
            send(viewport, down, down + 96L, MotionEvent.ACTION_UP, to[0], to[1]);
            return null;
        });
        settleLayout();
    }

    /** A rectangle drawn by drag, then Finish and Extrude. Returns the body. */
    static long drawRectangleAndExtrude(ActivityScenario<ForgeShapeActivity> scenario,
                                        double width, double height, String depth) {
        selectTool(scenario, R.id.tool_rail_rectangle);
        dragSketch(scenario, -width / 2, -height / 2, width / 2, height / 2);
        assertEquals("one rectangle placed", 1, sketchEntityCount());
        finishAndExtrude(scenario, depth);
        assertTrue("the extrusion is a CAD body", NativeViewport.sceneActiveBodyIsCad());
        return NativeViewport.sceneActiveBodyId();
    }

    static void tapViewport(ActivityScenario<ForgeShapeActivity> scenario, final float x,
                            final float y) {
        doOnWorkspace(scenario, (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final long down = SystemClock.uptimeMillis();
            send(viewport, down, down, MotionEvent.ACTION_DOWN, x, y);
            send(viewport, down, down + 40L, MotionEvent.ACTION_UP, x, y);
            return null;
        });
        settleLayout();
    }

    /**
     * A stylus HOVER sample at a pixel, through the viewport's real generic
     * motion dispatch — the path a hovering pen takes on hardware that has one.
     * Returns whether the viewport consumed it, which is native saying a
     * chooser target lit up.
     */
    static boolean hoverStylus(ActivityScenario<ForgeShapeActivity> scenario, final float x,
                               final float y) {
        final boolean[] consumed = new boolean[1];
        doOnWorkspace(scenario, (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final long now = SystemClock.uptimeMillis();
            final MotionEvent.PointerProperties props = new MotionEvent.PointerProperties();
            props.id = 0;
            props.toolType = MotionEvent.TOOL_TYPE_STYLUS;
            final MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
            coords.x = x;
            coords.y = y;
            final MotionEvent event = MotionEvent.obtain(now, now, MotionEvent.ACTION_HOVER_MOVE,
                    1, new MotionEvent.PointerProperties[]{props},
                    new MotionEvent.PointerCoords[]{coords}, 0, 0, 1f, 1f, 0, 0,
                    android.view.InputDevice.SOURCE_STYLUS, 0);
            try {
                consumed[0] = viewport.dispatchGenericMotionEvent(event);
            } finally {
                event.recycle();
            }
            return null;
        });
        settleLayout();
        return consumed[0];
    }

    static void onNativeStateChanged(ActivityScenario<ForgeShapeActivity> scenario) {
        doOnWorkspace(scenario, (activity, workspace) -> {
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    static int sketchState() {
        final double[] state = new double[NativeViewport.SKETCH_STATE_SIZE];
        NativeViewport.sketchState(state);
        return (int) state[NativeViewport.SKETCH_STATE];
    }

    /**
     * One tap at the pixel a SKETCH point projects to.
     *
     * <p>The pixel comes from {@code sketchScreenPoint} — the same projection
     * native unprojects with — so a tap lands on the sketch coordinate it names
     * whatever the window size, the zoom or the view rotation.
     */
    static void tapSketch(ActivityScenario<ForgeShapeActivity> scenario, double u, double v) {
        final float[] at = new float[2];
        assertTrue("the sketch point projects on screen",
                NativeViewport.sketchScreenPoint(u, v, at));
        tapViewport(scenario, at[0], at[1]);
    }

    /** Which workplane the open sketch is authored on, as a {@code WORKPLANE_*}. */
    static int sketchPlane() {
        final double[] state = new double[NativeViewport.SKETCH_STATE_SIZE];
        NativeViewport.sketchState(state);
        return (int) state[NativeViewport.SKETCH_PLANE];
    }

    /** The orientation navigator's state, re-read from the session. */
    static double[] sketchViewState() {
        final double[] state = new double[NativeViewport.SKETCH_VIEW_SIZE];
        NativeViewport.sketchViewState(state);
        return state;
    }

    /**
     * Presses one of the navigator's controls, the way a finger does.
     *
     * <p>By ID, never by coordinate: the navigator moves with the window, and a
     * pixel is only ever true for one run.
     */
    static void pressNavigator(ActivityScenario<ForgeShapeActivity> scenario, final int id) {
        doOnWorkspace(scenario, (activity, workspace) -> {
            final View control = workspace.findViewById(id);
            assertTrue("the navigator control is on screen", control != null && control.isShown());
            control.performClick();
            return null;
        });
        settleLayout();
    }

    static int sketchEntityCount() {
        final double[] state = new double[NativeViewport.SKETCH_STATE_SIZE];
        NativeViewport.sketchState(state);
        return (int) state[NativeViewport.SKETCH_ENTITY_COUNT];
    }

    private static void send(View target, long downTime, long eventTime, int action, float x,
                             float y) {
        final MotionEvent event = MotionEvent.obtain(downTime, eventTime, action, x, y, 0);
        try {
            target.dispatchTouchEvent(event);
        } finally {
            event.recycle();
        }
    }
}
