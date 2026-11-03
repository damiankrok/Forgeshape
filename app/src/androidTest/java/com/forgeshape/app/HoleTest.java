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
import android.widget.EditText;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * `CAD-HOLE-R1` on the device: a tap on a CAD body's face offers Hole beside
 * Sketch here; Hole opens the editor seeded with a round diameter and Through
 * All, and Apply drills it as ONE Undo; a Counterbore to a typed depth shows
 * exactly its own rows; and the History reopens a hole for an edit or a
 * Delete.
 *
 * <p>Every viewport touch is a real window touch through the decor view;
 * results are asserted from native truth (the offer, the hole's values, the
 * timeline, the undo depth). Emulator evidence closes no physical-device gate.
 */
@RunWith(AndroidJUnit4.class)
public final class HoleTest {

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
    public void a_face_offers_hole_and_apply_drills_it_as_one_undo() {
        final long body = block();
        tapTop();
        assertTrue("Hole is drawn", shown(R.id.cad_canvas_hole));
        assertTrue("Sketch here stays beside it", shown(R.id.cad_canvas_sketch_on_face));

        final int undoBefore = NativeViewport.constructionUndoDepth();
        realTap(centre(windowRect(R.id.cad_canvas_hole)));
        assertTrue("Hole opens the editor", shown(R.id.cad_canvas_hole_editor));
        assertTrue("a Simple hole first", activated(R.id.hole_kind_simple));
        assertTrue("Through All first", activated(R.id.hole_through_all));
        assertFalse("Through All has no depth to type", shown(R.id.field_hole_depth));
        assertFalse("a Simple hole has no bore row", shown(R.id.field_hole_bore_diameter));
        assertFalse("nor a cone row", shown(R.id.field_hole_sink_diameter));
        assertFalse("a new hole has nothing to delete", shown(R.id.delete_hole));
        press(R.id.apply_hole);
        assertEquals("Apply is ONE undo step", undoBefore + 1, NativeViewport.constructionUndoDepth());
        assertEquals("the body carries one hole", 1, holeRows(body));
        assertFalse("the editor closes on success", shown(R.id.cad_canvas_hole_editor));
        final double[] info = new double[NativeViewport.HOLE_INFO_SIZE];
        assertTrue(NativeViewport.cadHoleInfo(body, lastHoleId(body), info));
        assertEquals(NativeViewport.HOLE_SIMPLE, (int) info[NativeViewport.HOLE_INFO_KIND]);
        assertEquals("Through All", 1.0, info[NativeViewport.HOLE_INFO_THROUGH_ALL], 0.0);
        assertEquals("a fifth of the 1 m thickness, rounded", 0.2,
                info[NativeViewport.HOLE_INFO_DIAMETER], 1e-9);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.constructionUndo();
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        assertEquals("Undo takes the hole back", 0, holeRows(body));
    }

    @Test
    public void a_counterbore_to_a_depth_and_history_edits_and_deletes_it() {
        final long body = block();
        tapTop();
        realTap(centre(windowRect(R.id.cad_canvas_hole)));
        press(R.id.hole_kind_counterbore);
        assertTrue("a Counterbore draws its bore row", shown(R.id.field_hole_bore_diameter));
        assertFalse("and no cone row", shown(R.id.field_hole_sink_diameter));
        press(R.id.hole_blind);
        assertTrue("a blind hole has its depth", shown(R.id.field_hole_depth));
        press(R.id.apply_hole);
        assertEquals("one hole", 1, holeRows(body));
        final long holeId = lastHoleId(body);
        final double[] info = new double[NativeViewport.HOLE_INFO_SIZE];
        assertTrue(NativeViewport.cadHoleInfo(body, holeId, info));
        assertEquals(NativeViewport.HOLE_COUNTERBORE, (int) info[NativeViewport.HOLE_INFO_KIND]);
        assertEquals("to a depth", 0.0, info[NativeViewport.HOLE_INFO_THROUGH_ALL], 0.0);
        assertTrue("the seeded bore is wider than the hole",
                info[NativeViewport.HOLE_INFO_BORE_DIAMETER] > info[NativeViewport.HOLE_INFO_DIAMETER]);

        final int depth = NativeViewport.constructionUndoDepth();
        final boolean opened = onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.cadExtrudeCanvas().beginHoleEdit(body, holeId));
        settleLayout();
        assertTrue("the hole reopens for an edit", opened);
        assertTrue("an existing hole can be deleted", shown(R.id.delete_hole));
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final EditText field = activity.findViewById(R.id.field_hole_diameter);
            field.setText(workspace.uiState().displayUnit().format(0.1));
            return null;
        });
        press(R.id.apply_hole);
        assertEquals("an edit is one step", depth + 1, NativeViewport.constructionUndoDepth());
        assertTrue(NativeViewport.cadHoleInfo(body, holeId, info));
        assertEquals("the typed diameter is what was applied", 0.1,
                info[NativeViewport.HOLE_INFO_DIAMETER], 1e-9);

        assertTrue(onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.cadExtrudeCanvas().beginHoleEdit(body, holeId)));
        settleLayout();
        press(R.id.delete_hole);
        assertEquals("Delete is one step", depth + 2, NativeViewport.constructionUndoDepth());
        assertEquals("and removes the hole", 0, holeRows(body));
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

    /** Taps the block's top face; the face offers a hole. */
    private void tapTop() {
        setCamera(0.7f, 0.9f, 8.0f);
        realTap(worldToWindow(0.3, 0.3, 1.0));
        final double[] offer = new double[NativeViewport.HOLE_OFFER_SIZE];
        assertTrue("a tap on the top offers a hole", NativeViewport.holeOfferState(offer));
    }

    private static int holeRows(long body) {
        return holeRowIds(body).size();
    }

    private static long lastHoleId(long body) {
        final java.util.List<Long> ids = holeRowIds(body);
        return ids.isEmpty() ? 0L : ids.get(ids.size() - 1);
    }

    private static java.util.List<Long> holeRowIds(long body) {
        final double[] header = new double[NativeViewport.TIMELINE_HEADER_SIZE];
        final double[] rows =
                new double[NativeViewport.TIMELINE_MAX_ROWS * NativeViewport.TIMELINE_ROW_SIZE];
        final int count = NativeViewport.cadTimeline(body, false, header, rows);
        final java.util.List<Long> ids = new java.util.ArrayList<>();
        for (int i = 0; i < count && i < NativeViewport.TIMELINE_MAX_ROWS; i++) {
            final int at = i * NativeViewport.TIMELINE_ROW_SIZE;
            if ((int) rows[at + NativeViewport.TIMELINE_ROW_KIND]
                    == NativeViewport.TIMELINE_KIND_HOLE) {
                ids.add((long) rows[at + NativeViewport.TIMELINE_ROW_ID]);
            }
        }
        return ids;
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
