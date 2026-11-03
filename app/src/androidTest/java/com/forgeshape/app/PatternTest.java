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
 * `CAD-PATTERN-R1` on the device: a tap on a boss (a later Add) offers
 * Pattern and Mirror beside Sketch here; Pattern opens the editor seeded with a
 * run that fits the part, and Apply repeats the boss as ONE Undo; Mirror
 * reflects it across the part's centre plane; and the History reopens a
 * pattern for an edit or a Delete.
 *
 * <p>Every viewport touch is a real window touch through the decor view;
 * results are asserted from native truth (the offer, the timeline, the undo
 * depth). Emulator evidence closes no physical-device gate.
 */
@RunWith(AndroidJUnit4.class)
public final class PatternTest {

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
    public void a_boss_offers_pattern_and_apply_repeats_it_as_one_undo() {
        final long body = blockWithBoss();
        final double[] offer = tapBoss();
        assertEquals("the boss is the feature offered", 2.0,
                offer[NativeViewport.PATTERN_OFFER_FEATURE], 0.0);
        assertEquals(1.0, offer[NativeViewport.PATTERN_OFFER_OPERATION], 0.0);
        assertTrue("Pattern is drawn", shown(R.id.cad_canvas_pattern));
        assertTrue("Mirror is drawn", shown(R.id.cad_canvas_mirror_feature));
        assertTrue("Sketch here stays beside them", shown(R.id.cad_canvas_sketch_on_face));

        final int undoBefore = NativeViewport.constructionUndoDepth();
        realTap(centre(windowRect(R.id.cad_canvas_pattern)));
        assertTrue("Pattern opens the editor", shown(R.id.cad_canvas_pattern_editor));
        assertTrue("a Linear pattern first", activated(R.id.pattern_kind_linear));
        assertTrue("along a direction in the boss's own plane", activated(R.id.pattern_axis_x));
        assertFalse("a new pattern has nothing to delete", shown(R.id.delete_pattern));
        assertEquals("the run that fits the part", "2", text(R.id.field_pattern_count));
        press(R.id.apply_pattern);
        assertEquals("Apply is ONE undo step", undoBefore + 1, NativeViewport.constructionUndoDepth());
        assertEquals("the body carries one pattern", 1, patternRows(body));
        assertFalse("the editor closes on success", shown(R.id.cad_canvas_pattern_editor));

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.constructionUndo();
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        assertEquals("Undo takes the pattern back", 0, patternRows(body));
    }

    @Test
    public void mirror_reflects_the_boss_and_history_deletes_it() {
        final long body = blockWithBoss();
        tapBoss();
        realTap(centre(windowRect(R.id.cad_canvas_mirror_feature)));
        assertTrue(shown(R.id.cad_canvas_pattern_editor));
        assertTrue("Mirror opens on the mirror kind", activated(R.id.pattern_kind_mirror));
        assertFalse("a mirror has no count", shown(R.id.field_pattern_count));
        press(R.id.apply_pattern);
        assertEquals("one mirror", 1, patternRows(body));
        final long patternId = lastPatternId(body);
        final double[] info = new double[NativeViewport.PATTERN_INFO_SIZE];
        assertTrue(NativeViewport.cadPatternInfo(body, patternId, info));
        assertEquals(NativeViewport.PATTERN_MIRROR, (int) info[NativeViewport.PATTERN_INFO_KIND]);
        assertEquals(2.0, info[NativeViewport.PATTERN_INFO_INSTANCES], 0.0);

        final int depth = NativeViewport.constructionUndoDepth();
        final boolean opened = onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.cadExtrudeCanvas().beginPatternEdit(body, patternId));
        settleLayout();
        assertTrue("the pattern reopens for an edit", opened);
        assertTrue("an existing pattern can be deleted", shown(R.id.delete_pattern));
        press(R.id.delete_pattern);
        assertEquals("Delete is one step", depth + 1, NativeViewport.constructionUndoDepth());
        assertEquals("and removes the pattern", 0, patternRows(body));
    }

    // -----------------------------------------------------------------------

    /** A 2 x 2 x 1 block with a round boss (r 0.25, 0.5 tall) at x = -0.5 on its top. */
    private long blockWithBoss() {
        press(R.id.home_new_project);
        press(R.id.new_project_cad);
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
        final long body = drawRectangleAndExtrude(rule.getScenario(), 2.0, 2.0, "1");
        assertTrue(body > 0);
        setCamera(0.7f, 0.9f, 8.0f);
        realTap(worldToWindow(0.3, 0.3, 1.0));
        assertTrue("the top face offers a sketch", shown(R.id.cad_canvas_sketch_on_face));
        press(R.id.cad_canvas_sketch_on_face);
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
        selectTool(rule.getScenario(), R.id.tool_rail_circle);
        dragSketch(rule.getScenario(), -0.5, 0.0, -0.25, 0.0);
        press(R.id.finish_sketch);
        assertEquals(NativeViewport.SKETCH_READY, sketchState());
        assertEquals(NativeViewport.CAD_OK, NativeViewport.sketchSetOperation(NativeViewport.OPERATION_ADD));
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final EditText field = workspace.sketchEditor().findViewById(R.id.field_extrude_depth);
            field.setText("0.5");
            workspace.findViewById(R.id.sketch_extrude_commit).performClick();
            return null;
        });
        settleLayout();
        assertEquals("the boss is committed", NativeViewport.SKETCH_INACTIVE, sketchState());
        return body;
    }

    /** Taps the boss's top and returns the Pattern offer it makes. */
    private double[] tapBoss() {
        setCamera(0.7f, 0.9f, 8.0f);
        realTap(worldToWindow(-0.5, 0.0, 1.5));
        final double[] offer = new double[NativeViewport.PATTERN_OFFER_SIZE];
        assertTrue("a tap on the boss offers Pattern and Mirror",
                NativeViewport.patternOfferState(offer));
        return offer;
    }

    private static int patternRows(long body) {
        return patternRowIds(body).size();
    }

    private static long lastPatternId(long body) {
        final java.util.List<Long> ids = patternRowIds(body);
        return ids.isEmpty() ? 0L : ids.get(ids.size() - 1);
    }

    private static java.util.List<Long> patternRowIds(long body) {
        final double[] header = new double[NativeViewport.TIMELINE_HEADER_SIZE];
        final double[] rows =
                new double[NativeViewport.TIMELINE_MAX_ROWS * NativeViewport.TIMELINE_ROW_SIZE];
        final int count = NativeViewport.cadTimeline(body, false, header, rows);
        final java.util.List<Long> ids = new java.util.ArrayList<>();
        for (int i = 0; i < count && i < NativeViewport.TIMELINE_MAX_ROWS; i++) {
            final int at = i * NativeViewport.TIMELINE_ROW_SIZE;
            if ((int) rows[at + NativeViewport.TIMELINE_ROW_KIND]
                    == NativeViewport.TIMELINE_KIND_PATTERN) {
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
