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
 * `CAD-TO-FACE-R1` on the device. On a sketch drawn on a CAD part's face, To
 * Face is absent for New Body and offered once the operation is Add (with a
 * face of the part to reach); choosing it ends the extrusion on the nearest
 * parallel face -- a post's top -- reports that distance, and the precision
 * surface's Extrude commits it as To Face in ONE undo step. A hole switched to
 * From edges is placed by two distances native seeds from where it stood, and
 * Apply keeps it placed from them.
 *
 * <p>Results are asserted from native truth (the tool state, the timeline, the
 * hole's values, the undo depth). Emulator evidence closes no physical-device
 * gate.
 */
@RunWith(AndroidJUnit4.class)
public final class ToFaceTest {

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
    public void an_add_offers_to_face_and_commits_it_ending_on_the_post_top() {
        final long body = block();
        // A post, 1 m tall, on one quadrant of the top.
        sketchOnTop(0.3, 0.3);
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), 0.35, 0.35, 0.75, 0.75);
        press(R.id.finish_sketch);
        assertEquals(NativeViewport.CAD_OK,
                NativeViewport.sketchSetOperation(NativeViewport.OPERATION_ADD));
        final double[] tool = new double[NativeViewport.CAD_EXTRUDE_SIZE];
        NativeViewport.cadExtrudeToolState(tool);
        assertEquals(NativeViewport.CAD_OK, NativeViewport.sketchSetExtrude(1.0,
                (int) tool[NativeViewport.CAD_EXTRUDE_DIRECTION]));
        commitFromPrecisionSurface();
        assertEquals("the post is committed", NativeViewport.SKETCH_INACTIVE, sketchState());

        // A boss beside it, up to the post's top.
        sketchOnTop(0.0, -0.5);
        selectTool(rule.getScenario(), R.id.tool_rail_circle);
        dragSketch(rule.getScenario(), 0.55, -0.55, 0.75, -0.55);
        press(R.id.finish_sketch);
        assertEquals(NativeViewport.SKETCH_READY, sketchState());
        refresh();
        assertEquals("New Body has no face of this part to end on", View.GONE,
                choiceVisibility(R.id.cad_extrude_extent_to_face));
        assertEquals(NativeViewport.CAD_OK,
                NativeViewport.sketchSetOperation(NativeViewport.OPERATION_ADD));
        refresh();
        assertEquals("an Add offers To Face", View.VISIBLE,
                choiceVisibility(R.id.cad_extrude_extent_to_face));
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.cadExtrudeCanvas().findViewById(R.id.cad_extrude_extent_to_face)
                    .performClick();
            return null;
        });
        settleLayout();
        NativeViewport.cadExtrudeToolState(tool);
        assertEquals("the extent is To Face", NativeViewport.EXTENT_TO_FACE,
                (int) tool[NativeViewport.CAD_EXTRUDE_EXTENT]);
        assertEquals("it ends on the post's top, 1 m up", 1.0,
                tool[NativeViewport.CAD_EXTRUDE_DEPTH], 1e-4);
        assertEquals("the candidate would commit", NativeViewport.CAD_OK,
                (int) tool[NativeViewport.CAD_EXTRUDE_CANDIDATE_STATUS]);

        final int undoBefore = NativeViewport.constructionUndoDepth();
        commitFromPrecisionSurface();
        assertEquals("the boss is committed", NativeViewport.SKETCH_INACTIVE, sketchState());
        assertEquals("as ONE undo step", undoBefore + 1, NativeViewport.constructionUndoDepth());
        assertEquals("and stays To Face, not the number it showed",
                NativeViewport.EXTENT_TO_FACE, lastFeatureExtent(body));
    }

    @Test
    public void a_hole_switched_to_from_edges_is_placed_and_kept_from_them() {
        final long body = block();
        setCamera(0.7f, 0.9f, 8.0f);
        realTap(worldToWindow(0.3, 0.3, 1.0));
        final double[] offer = new double[NativeViewport.HOLE_OFFER_SIZE];
        assertTrue("a tap on the top offers a hole", NativeViewport.holeOfferState(offer));
        realTap(centre(windowRect(R.id.cad_canvas_hole)));
        assertTrue("Hole opens the editor", shown(R.id.cad_canvas_hole_editor));
        assertTrue("a new hole is placed at a point", activated(R.id.hole_place_point));
        assertFalse("with no edge rows", shown(R.id.field_hole_edge_a));
        press(R.id.hole_place_edges);
        assertTrue("From edges is chosen", activated(R.id.hole_place_edges));
        assertTrue("its two rows are drawn", shown(R.id.field_hole_edge_a)
                && shown(R.id.field_hole_edge_b));
        assertFalse("and the point rows are not", shown(R.id.field_hole_u));
        final double a = fieldMeters(R.id.field_hole_edge_a);
        final double b = fieldMeters(R.id.field_hole_edge_b);
        assertTrue("the seeds stand inside the face: " + a + ", " + b,
                a > 0.0 && a < 2.0 && b > 0.0 && b < 2.0);

        final int undoBefore = NativeViewport.constructionUndoDepth();
        press(R.id.apply_hole);
        assertEquals("Apply is ONE undo step", undoBefore + 1, NativeViewport.constructionUndoDepth());
        final double[] info = new double[NativeViewport.HOLE_INFO_SIZE];
        assertTrue(NativeViewport.cadHoleInfo(body, lastHoleId(body), info));
        assertEquals("the hole is placed from edges", 1.0,
                info[NativeViewport.HOLE_INFO_BY_EDGES], 0.0);
        assertEquals(a, info[NativeViewport.HOLE_INFO_EDGE_A], 1e-6);
        assertEquals(b, info[NativeViewport.HOLE_INFO_EDGE_B], 1e-6);
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

    /**
     * Taps the block's top at (x, y) and takes Sketch here. Seen from nearly
     * straight above, so a post standing on the top cannot hide the point.
     */
    private void sketchOnTop(double x, double y) {
        setCamera(0.7f, 1.35f, 8.0f);
        realTap(worldToWindow(x, y, 1.0));
        assertTrue("the top face offers a sketch", shown(R.id.cad_canvas_sketch_on_face));
        press(R.id.cad_canvas_sketch_on_face);
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
    }

    private void commitFromPrecisionSurface() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            // The pinned Extrude SUBMITS its depth field first: refresh it
            // from native, as the shell does after every act, so a depth set
            // below JNI is the one it submits.
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
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
