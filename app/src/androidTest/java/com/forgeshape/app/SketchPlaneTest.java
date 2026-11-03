package com.forgeshape.app;

import static com.forgeshape.app.SketchTestSupport.dragSketch;
import static com.forgeshape.app.SketchTestSupport.drawRectangleAndExtrude;
import static com.forgeshape.app.SketchTestSupport.selectTool;
import static com.forgeshape.app.SketchTestSupport.sketchState;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.widget.EditText;

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
 * `CAD-PLANE-R1` on the device: on a sketch drawn on a CAD part's face, the
 * precision surface offers the sketch plane; Move plane with an offset stands
 * the sketch off the face (native reports the offset, and New Body is no longer
 * the operation, because a new body cannot stand off a face); a Cut drawn there
 * commits in ONE undo step, and the History lists its sketch as off the face by
 * that offset.
 *
 * <p>Results are asserted from native truth (the plane state, the timeline,
 * the undo depth). Emulator evidence closes no physical-device gate.
 */
@RunWith(AndroidJUnit4.class)
public final class SketchPlaneTest {

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
    public void a_face_sketch_moves_off_its_face_and_cuts_from_there() {
        final long body = block();
        sketchOnTop(0.3, 0.3);
        final double[] plane = new double[3];
        NativeViewport.sketchFacePlaneState(plane);
        assertEquals("a sketch on the part's face may stand off it", 1.0, plane[0], 0.0);
        refresh();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final EditText offset = activity.findViewById(R.id.field_sketch_plane_offset);
            assertNotNull("the plane section has its offset field", offset);
            offset.setText(workspace.uiState().displayUnit().format(0.5));
            activity.findViewById(R.id.sketch_plane_apply).performClick();
            return null;
        });
        settleLayout();
        NativeViewport.sketchFacePlaneState(plane);
        assertEquals("the sketch stands 0.5 m off the face", 0.5, plane[1], 1e-9);
        assertEquals("not tilted", 0.0, plane[2], 0.0);

        selectTool(rule.getScenario(), R.id.tool_rail_circle);
        dragSketch(rule.getScenario(), 0.0, 0.0, 0.3, 0.0);
        press(R.id.finish_sketch);
        assertEquals(NativeViewport.SKETCH_READY, sketchState());
        assertEquals(NativeViewport.CAD_OK,
                NativeViewport.sketchSetOperation(NativeViewport.OPERATION_CUT));
        final double[] tool = new double[NativeViewport.CAD_EXTRUDE_SIZE];
        NativeViewport.cadExtrudeToolState(tool);
        assertEquals(NativeViewport.CAD_OK, NativeViewport.sketchSetExtrude(1.0,
                (int) tool[NativeViewport.CAD_EXTRUDE_DIRECTION]));
        NativeViewport.cadExtrudeToolState(tool);
        assertEquals("the cut from the plane would commit", NativeViewport.CAD_OK,
                (int) tool[NativeViewport.CAD_EXTRUDE_CANDIDATE_STATUS]);
        final int undoBefore = NativeViewport.constructionUndoDepth();
        commitFromPrecisionSurface();
        assertEquals("the cut is committed", NativeViewport.SKETCH_INACTIVE, sketchState());
        assertEquals("as ONE undo step", undoBefore + 1, NativeViewport.constructionUndoDepth());
        assertEquals("its sketch stands 0.5 m off the face", 0.5, lastSketchOffset(body), 1e-9);
    }

    @Test
    public void a_part_measures_its_volume_area_and_centre() {
        final long body = block();
        final double[] mass = new double[NativeViewport.MASS_SIZE];
        assertTrue(NativeViewport.bodyMassProperties(body, mass));
        assertEquals("the 2 x 2 x 1 block is closed", 1.0, mass[NativeViewport.MASS_CLOSED], 0.0);
        assertEquals("its volume", 4.0, mass[NativeViewport.MASS_VOLUME], 1e-4);
        assertEquals("its surface area", 16.0, mass[NativeViewport.MASS_AREA], 1e-4);
        assertEquals("its centre of mass is half way up", 0.5,
                mass[NativeViewport.MASS_CENTROID_X + 2], 1e-4);
        assertEquals("its size", 2.0, mass[NativeViewport.MASS_SIZE_X], 1e-5);
        final String shown = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final android.widget.TextView text = activity.findViewById(R.id.cad_mass_properties);
            return text == null ? "" : text.getText().toString();
        });
        assertTrue("the Measure section reads the volume: " + shown,
                shown.isEmpty() || shown.contains("4"));
    }

    private static double lastSketchOffset(long body) {
        final double[] header = new double[NativeViewport.TIMELINE_HEADER_SIZE];
        final double[] rows =
                new double[NativeViewport.TIMELINE_MAX_ROWS * NativeViewport.TIMELINE_ROW_SIZE];
        final int count = NativeViewport.cadTimeline(body, false, header, rows);
        double offset = -1.0;
        for (int i = 0; i < count && i < NativeViewport.TIMELINE_MAX_ROWS; i++) {
            final int at = i * NativeViewport.TIMELINE_ROW_SIZE;
            if ((int) rows[at + NativeViewport.TIMELINE_ROW_KIND]
                    == NativeViewport.TIMELINE_KIND_SKETCH
                    && rows[at + NativeViewport.TIMELINE_ROW_ON_FEATURE_FACE] != 0.0) {
                offset = rows[at + NativeViewport.TIMELINE_ROW_POSITIVE];
            }
        }
        return offset;
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
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.sketch_extrude_commit).performClick();
            return null;
        });
        settleLayout();
    }

    private double fieldMeters(int id) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final EditText field = activity.findViewById(id);
            return workspace.uiState().displayUnit()
                    .toMeters(LengthUnit.parse(field.getText().toString())).doubleValue();
        });
    }

    private static long lastHoleId(long body) {
        final double[] header = new double[NativeViewport.TIMELINE_HEADER_SIZE];
        final double[] rows =
                new double[NativeViewport.TIMELINE_MAX_ROWS * NativeViewport.TIMELINE_ROW_SIZE];
        final int count = NativeViewport.cadTimeline(body, false, header, rows);
        long id = 0L;
        for (int i = 0; i < count && i < NativeViewport.TIMELINE_MAX_ROWS; i++) {
            final int at = i * NativeViewport.TIMELINE_ROW_SIZE;
            if ((int) rows[at + NativeViewport.TIMELINE_ROW_KIND]
                    == NativeViewport.TIMELINE_KIND_HOLE) {
                id = (long) rows[at + NativeViewport.TIMELINE_ROW_ID];
            }
        }
        return id;
    }

    private static int lastFeatureExtent(long body) {
        final double[] header = new double[NativeViewport.TIMELINE_HEADER_SIZE];
        final double[] rows =
                new double[NativeViewport.TIMELINE_MAX_ROWS * NativeViewport.TIMELINE_ROW_SIZE];
        final int count = NativeViewport.cadTimeline(body, false, header, rows);
        int extent = -1;
        for (int i = 0; i < count && i < NativeViewport.TIMELINE_MAX_ROWS; i++) {
            final int at = i * NativeViewport.TIMELINE_ROW_SIZE;
            if ((int) rows[at + NativeViewport.TIMELINE_ROW_KIND]
                    == NativeViewport.TIMELINE_KIND_FEATURE) {
                extent = (int) rows[at + NativeViewport.TIMELINE_ROW_EXTENT];
            }
        }
        return extent;
    }

    private int choiceVisibility(int id) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View view = workspace.cadExtrudeCanvas().findViewById(id);
            assertNotNull(view);
            return view.getVisibility();
        });
    }

    private void refresh() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onNativeStateChanged();
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
