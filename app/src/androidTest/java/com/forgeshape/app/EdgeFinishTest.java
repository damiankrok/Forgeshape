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
 * `CAD-EDGE-FINISH-R1` on the device: a tap beside a straight edge of a CAD
 * body offers Fillet and Chamfer there, the editor opens seeded with a value
 * that fits, Apply rounds the edge as ONE Undo and Undo takes it back; a tap on
 * the middle of a face offers to round every edge of that face at once; and
 * the History lists the finish and reopens it for an edit or a Delete.
 *
 * <p>Every viewport touch is a real window touch through the decor view.
 * Asserted from native truth (the offer, the timeline, the undo depth).
 * Emulator evidence closes no physical-device gate.
 */
@RunWith(AndroidJUnit4.class)
public final class EdgeFinishTest {

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
    public void a_tap_beside_an_edge_offers_a_fillet_and_apply_is_one_undo() {
        newCadProjectBlock();
        setCamera(0.7f, 0.9f, 8.0f);
        final int undoBefore = NativeViewport.constructionUndoDepth();
        final double[] offer = new double[NativeViewport.EDGE_OFFER_SIZE];
        // A point on the visible top face, a hair inside each of its edges:
        // one of them is in view from this camera.
        boolean offered = false;
        final double[][] beside = {{0.0, -0.96}, {0.96, 0.0}, {0.0, 0.96}, {-0.96, 0.0}};
        for (double[] p : beside) {
            realTap(worldToWindow(p[0], p[1], 1.0));
            if (NativeViewport.edgeFinishOfferState(offer)
                    && offer[NativeViewport.EDGE_OFFER_MODE] == NativeViewport.EDGE_OFFER_EDGE) {
                offered = true;
                break;
            }
        }
        assertTrue("a tap beside a top edge offers that edge", offered);
        assertEquals(1.0, offer[NativeViewport.EDGE_OFFER_COUNT], 0.0);
        assertTrue("a fillet that fits is suggested",
                offer[NativeViewport.EDGE_OFFER_SUGGEST_FILLET] > 0.0
                        && offer[NativeViewport.EDGE_OFFER_SUGGEST_FILLET] < 1.0);
        assertTrue("the Fillet / Chamfer bar is drawn", shown(R.id.cad_canvas_edge_finish_bar));
        assertFalse("an edge is not a face: no Sketch here", shown(R.id.cad_canvas_sketch_on_face));
        assertEquals("an offer records nothing", undoBefore, NativeViewport.constructionUndoDepth());

        realTap(centre(windowRect(R.id.cad_canvas_edge_fillet)));
        assertTrue("Fillet opens the editor", shown(R.id.cad_canvas_edge_finish_editor));
        assertFalse("a new finish has nothing to delete", shown(R.id.delete_edge_finish));
        press(R.id.apply_edge_finish);
        assertEquals("Apply is ONE undo step", undoBefore + 1, NativeViewport.constructionUndoDepth());
        assertEquals("the body carries one finish", 1, finishRows());
        assertFalse("the editor closes on success", shown(R.id.cad_canvas_edge_finish_editor));

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.constructionUndo();
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        assertEquals("Undo takes the finish back", 0, finishRows());
    }

    @Test
    public void a_tap_on_a_face_offers_to_round_all_its_edges_and_history_edits_it() {
        newCadProjectBlock();
        setCamera(0.7f, 0.9f, 8.0f);
        realTap(worldToWindow(0.3, 0.3, 1.0));
        final double[] offer = new double[NativeViewport.EDGE_OFFER_SIZE];
        assertTrue("a face tap makes an offer", NativeViewport.edgeFinishOfferState(offer));
        assertEquals("it is the face's edges", NativeViewport.EDGE_OFFER_FACE,
                (int) offer[NativeViewport.EDGE_OFFER_MODE]);
        assertEquals("all four of them", 4.0, offer[NativeViewport.EDGE_OFFER_COUNT], 0.0);
        assertTrue("Sketch here stays offered beside it", shown(R.id.cad_canvas_sketch_on_face));
        assertTrue(shown(R.id.cad_canvas_edge_finish_bar));

        realTap(centre(windowRect(R.id.cad_canvas_edge_chamfer)));
        assertTrue(shown(R.id.cad_canvas_edge_finish_editor));
        press(R.id.apply_edge_finish);
        assertEquals("one chamfer over the whole loop", 1, finishRows());
        final long body = NativeViewport.sceneActiveBodyId();
        final long finishId = lastFinishId(body);
        final double[] info = new double[NativeViewport.EDGE_FINISH_INFO_SIZE];
        assertTrue(NativeViewport.cadEdgeFinishInfo(body, finishId, info));
        assertEquals(NativeViewport.EDGE_FINISH_CHAMFER, (int) info[NativeViewport.EDGE_FINISH_INFO_KIND]);
        assertEquals(4.0, info[NativeViewport.EDGE_FINISH_INFO_EDGES], 0.0);

        // Edit from the History's own row, then Delete it there.
        final int depth = NativeViewport.constructionUndoDepth();
        final boolean opened = onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.cadExtrudeCanvas().beginEdgeFinishEdit(body, finishId));
        settleLayout();
        assertTrue("the finish reopens for an edit", opened);
        assertTrue("an existing finish can be deleted", shown(R.id.delete_edge_finish));
        press(R.id.delete_edge_finish);
        assertEquals("Delete is one step", depth + 1, NativeViewport.constructionUndoDepth());
        assertEquals("and removes the finish", 0, finishRows());
    }

    private static int finishRows() {
        return finishRowIds(NativeViewport.sceneActiveBodyId()).size();
    }

    private static long lastFinishId(long body) {
        final java.util.List<Long> ids = finishRowIds(body);
        return ids.isEmpty() ? 0L : ids.get(ids.size() - 1);
    }

    private static java.util.List<Long> finishRowIds(long body) {
        final double[] header = new double[NativeViewport.TIMELINE_HEADER_SIZE];
        final double[] rows =
                new double[NativeViewport.TIMELINE_MAX_ROWS * NativeViewport.TIMELINE_ROW_SIZE];
        final int count = NativeViewport.cadTimeline(body, false, header, rows);
        final java.util.List<Long> ids = new java.util.ArrayList<>();
        for (int i = 0; i < count && i < NativeViewport.TIMELINE_MAX_ROWS; i++) {
            final int at = i * NativeViewport.TIMELINE_ROW_SIZE;
            if ((int) rows[at + NativeViewport.TIMELINE_ROW_KIND]
                    == NativeViewport.TIMELINE_KIND_FINISH) {
                ids.add((long) rows[at + NativeViewport.TIMELINE_ROW_ID]);
            }
        }
        return ids;
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
