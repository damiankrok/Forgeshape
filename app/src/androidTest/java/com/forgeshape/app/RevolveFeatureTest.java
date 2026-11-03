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
 * `CAD-REVOLVE-FEATURE-R1` on the device: on a sketch drawn on a CAD part's
 * face, Revolve now offers Add and Cut besides New Body; a full-turn Revolve
 * Cut of a small circle about a line in the face cuts a groove into the SAME
 * part as ONE undo step, the part's volume drops, and the History lists the
 * feature as a Revolve.
 *
 * <p>Results are asserted from native truth (the tool state, the timeline, the
 * mass properties, the undo depth). Emulator evidence closes no
 * physical-device gate.
 */
@RunWith(AndroidJUnit4.class)
public final class RevolveFeatureTest {

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
    public void a_revolve_on_a_face_cuts_a_groove_into_the_same_part() {
        final long body = block();
        final double[] mass = new double[NativeViewport.MASS_SIZE];
        assertTrue(NativeViewport.bodyMassProperties(body, mass));
        final double before = mass[NativeViewport.MASS_VOLUME];
        sketchOnTop(0.3, 0.3);
        selectTool(rule.getScenario(), R.id.tool_rail_circle);
        dragSketch(rule.getScenario(), 0.5, 0.0, 0.65, 0.0);
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        final long axis = placeLine(0.0, -0.6, 0.0, 0.6);
        press(R.id.finish_sketch);
        assertEquals("Finish succeeds: " + NativeViewport.sketchLastStatus(),
                NativeViewport.SKETCH_READY, sketchState());
        assertEquals(NativeViewport.CAD_OK, applyOnUi(NativeViewport::sketchBeginRevolve));
        assertEquals(NativeViewport.CAD_OK,
                applyOnUi(() -> NativeViewport.sketchSetRevolveAxis(axis, 0)));
        assertEquals(NativeViewport.CAD_OK,
                applyOnUi(() -> NativeViewport.sketchSetRevolveAngle(360.0)));
        final double[] tool = new double[NativeViewport.CAD_EXTRUDE_SIZE];
        NativeViewport.cadExtrudeToolState(tool);
        final int offered = (int) tool[NativeViewport.CAD_EXTRUDE_OPERATIONS_AVAILABLE];
        assertTrue("a revolve on the part's face offers Add",
                (offered & NativeViewport.OPERATION_BIT_ADD) != 0);
        assertTrue("and Cut", (offered & NativeViewport.OPERATION_BIT_CUT) != 0);
        assertEquals(NativeViewport.CAD_OK,
                applyOnUi(() -> NativeViewport.sketchSetOperation(NativeViewport.OPERATION_CUT)));
        refresh();
        final int undoBefore = NativeViewport.constructionUndoDepth();
        assertTrue("the toolbar's Revolve is drawn", shown(R.id.revolve_sketch));
        press(R.id.revolve_sketch);
        assertEquals("the groove is committed: " + NativeViewport.sketchLastStatus(),
                NativeViewport.SKETCH_INACTIVE, sketchState());
        assertEquals("as ONE undo step", undoBefore + 1, NativeViewport.constructionUndoDepth());
        assertEquals("into the SAME part", body, NativeViewport.sceneActiveBodyId());
        assertTrue(NativeViewport.bodyMassProperties(body, mass));
        assertTrue("the part lost material: " + mass[NativeViewport.MASS_VOLUME],
                mass[NativeViewport.MASS_VOLUME] < before - 1e-3);
        assertEquals("the History lists a Revolve", NativeViewport.FEATURE_KIND_REVOLVE,
                lastFeatureKind(body));
    }

    /** A line by a real drag, then typed to its exact endpoints. */
    private long placeLine(double u0, double v0, double u1, double v1) {
        dragSketch(rule.getScenario(), u0, v0, u1, v1);
        final double[] drawn = new double[NativeViewport.SKETCH_ENTITY_SIZE];
        assertTrue("the new line is selected", NativeViewport.sketchSelectedEntity(drawn));
        final long id = (long) drawn[NativeViewport.SKETCH_ENTITY_ID];
        assertEquals(NativeViewport.CAD_OK,
                applyOnUi(() -> NativeViewport.sketchApplyLine(id, u0, v0, u1, v1)));
        return id;
    }

    private int applyOnUi(java.util.function.IntSupplier act) {
        final int result = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int code = act.getAsInt();
            workspace.onNativeStateChanged();
            return code;
        });
        settleLayout();
        return result;
    }

    private static int lastFeatureKind(long body) {
        final double[] header = new double[NativeViewport.TIMELINE_HEADER_SIZE];
        final double[] rows =
                new double[NativeViewport.TIMELINE_MAX_ROWS * NativeViewport.TIMELINE_ROW_SIZE];
        final int count = NativeViewport.cadTimeline(body, false, header, rows);
        int kind = -1;
        for (int i = 0; i < count && i < NativeViewport.TIMELINE_MAX_ROWS; i++) {
            final int at = i * NativeViewport.TIMELINE_ROW_SIZE;
            if ((int) rows[at + NativeViewport.TIMELINE_ROW_KIND]
                    == NativeViewport.TIMELINE_KIND_FEATURE) {
                kind = (int) rows[at + NativeViewport.TIMELINE_ROW_FEATURE_KIND];
            }
        }
        return kind;
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
