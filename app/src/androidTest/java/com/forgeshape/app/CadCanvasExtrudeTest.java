package com.forgeshape.app;

import static com.forgeshape.app.SketchTestSupport.dragSketch;
import static com.forgeshape.app.SketchTestSupport.selectTool;
import static com.forgeshape.app.SketchTestSupport.sketchState;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.os.SystemClock;
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
 * `E2E-CADUXS1-01..09` and `E2E-CADUXS1C1-03..06`: the canvas-first control of
 * the extrusion, on a device.
 *
 * <h2>What this suite is for</h2>
 *
 * <p>The DOMAIN half — the anchors, the axis, the drag arithmetic, the frozen
 * basis, the camera-attached scale rule, the clamps and the flip — is proved by
 * the native `CADUXS1-*` cases in the sketch-UX self-test, which build their own
 * sessions and their own cameras. What is left, and what this covers, is
 * everything that can only be true on a device: that the cluster is actually
 * drawn at the arrow with a live value, that Flip is one tap beside the
 * geometry, that typing an exact value lands the same authored depth, that
 * Cancel leaves the durable project untouched, that Apply is the existing
 * one-transaction New Body path, that the retained sketch is then reachable in
 * ONE tap from the body itself, and that Add, Cut, Symmetric and Two Sides are
 * nowhere to be found.
 *
 * <p>Since `CAD-UX-S1-C1` it also covers the two views a sketch is seen
 * through — the exact support-normal one it is AUTHORED in, and the feature
 * preview Finish Sketch opens, in which the arrow can actually be dragged. See
 * {@link #e2eCaduxs1c1_03_finishSketchGivesAViewTheArrowCanActuallyBeDraggedIn}
 * and {@link #e2eCaduxs1c1_06_theSketchCameraReturnsAndTheNextPreviewIsUsableAgain}.
 *
 * <h2>Rules</h2>
 *
 * <p>No control is located by coordinate. The one place a pixel appears is a
 * viewport gesture, and every such pixel is asked for from native —
 * {@code sketchScreenPoint} for a sketch coordinate, {@code cadExtrudeToolState}
 * for the arrow's own tip — through the same projection native unprojects with,
 * never written down.
 */
@RunWith(AndroidJUnit4.class)
public final class CadCanvasExtrudeTest {

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private byte[] baselineProject;

    @Before
    public void setUp() {
        resetToBaselineConstruction(rule.getScenario());
        baselineProject = NativeViewport.encodeProject();
    }

    @After
    public void tearDown() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.sketchCancel();
            NativeViewport.enterConstructionMode();
            if (baselineProject != null) {
                NativeViewport.loadProject(baselineProject);
            }
            return null;
        });
        resetToBaselineConstruction(rule.getScenario());
    }

    // -----------------------------------------------------------------------
    // E2E-CADUXS1-01: the cluster appears at the arrow, and only in Ready
    // -----------------------------------------------------------------------

    @Test
    public void e2eCaduxs1_01_theClusterIsAbsentWhileDrawingAndPresentWhenReady() {
        beginSketchXy();
        drawRectangle(2.0, 1.0);

        // While the sketch is being DRAWN there is nothing to point at: the
        // single finger belongs to the drawing, and the cluster is ABSENT
        // rather than inert.
        assertEquals("no manipulator while Editing", 0.0, toolState()[
                NativeViewport.CAD_EXTRUDE_ACTIVE], 0.0);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("and the surface is withdrawn", View.GONE,
                    workspace.cadExtrudeCanvas().getVisibility());
            return null;
        });

        finishSketch();
        assertEquals("Ready", NativeViewport.SKETCH_READY, sketchState());
        final double[] state = toolState();
        assertEquals("E2E-CADUXS1-01: the manipulator is live in Ready", 1.0,
                state[NativeViewport.CAD_EXTRUDE_ACTIVE], 0.0);
        assertEquals("with the session's own depth", 1.0,
                state[NativeViewport.CAD_EXTRUDE_DEPTH], 1e-9);
        assertTrue("and an anchor that projects on screen",
                state[NativeViewport.CAD_EXTRUDE_ON_SCREEN] != 0.0);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View canvas = workspace.cadExtrudeCanvas();
            assertEquals("the cluster is drawn", View.VISIBLE, canvas.getVisibility());
            final View value = canvas.findViewById(R.id.cad_extrude_depth_value);
            final View flip = canvas.findViewById(R.id.cad_extrude_flip);
            final View badge = canvas.findViewById(R.id.cad_extrude_operation);
            assertNotNull("with the exact value", value);
            assertNotNull("a direct Flip", flip);
            assertNotNull("and the operation badge", badge);
            assertEquals("which reads New Body",
                    activity.getString(R.string.extrude_operation_new_body),
                    ((android.widget.TextView) badge).getText().toString());
            // E2E-CADUXS1-09 in part: the 48 dp floor holds for the SMALLEST
            // the cluster can be drawn, which is the authored size times the
            // scale rule's own floor.
            final float density = activity.getResources().getDisplayMetrics().density;
            final int floor = Math.round(48f * density);
            final int authored = activity.getResources()
                    .getDimensionPixelSize(R.dimen.cad_canvas_control);
            assertTrue("the authored control times the minimum scale clears 48 dp",
                    Math.round(authored * 0.80f) >= floor - 1);
            assertTrue("the value control is a real target", value.getHeight() >= floor - 1);
            assertTrue("and so is Flip", flip.getHeight() >= floor - 1);
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // E2E-CADUXS1-02: Flip
    // -----------------------------------------------------------------------

    @Test
    public void e2eCaduxs1_02_flipChangesTheSideAndKeepsTheExactDepth() {
        beginSketchXy();
        drawRectangle(2.0, 1.0);
        finishSketch();
        setDepthThroughPanel("1.75");

        final double[] before = toolState();
        assertEquals("along the normal to start",
                NativeViewport.EXTRUDE_ALONG_NORMAL,
                (int) before[NativeViewport.CAD_EXTRUDE_DIRECTION]);
        final double depthBefore = before[NativeViewport.CAD_EXTRUDE_DEPTH];
        final long profileBefore = (long) before[NativeViewport.CAD_EXTRUDE_PROFILE];

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.cadExtrudeCanvas().findViewById(R.id.cad_extrude_flip).performClick();
            return null;
        });
        settleLayout();

        final double[] after = toolState();
        assertEquals("E2E-CADUXS1-02: Flip reverses the side",
                NativeViewport.EXTRUDE_AGAINST_NORMAL,
                (int) after[NativeViewport.CAD_EXTRUDE_DIRECTION]);
        assertEquals("the exact depth is unchanged", depthBefore,
                after[NativeViewport.CAD_EXTRUDE_DEPTH], 0.0);
        assertTrue("and it is still positive", after[NativeViewport.CAD_EXTRUDE_DEPTH] > 0.0);
        assertEquals("the same profile is still named", profileBefore,
                (long) after[NativeViewport.CAD_EXTRUDE_PROFILE]);

        // The panel chip and the canvas Flip are two views of ONE direction, not
        // two answers. The panel holds no draft since `CAD-UX-S1`, so refreshing
        // it makes it show what NATIVE says — which is the direction the canvas
        // just wrote. `setChipActive` is what verification reads.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.sketchEditor().refreshFromNative();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View along = workspace.sketchEditor()
                    .findViewById(R.id.extrude_direction_along);
            final View against = workspace.sketchEditor()
                    .findViewById(R.id.extrude_direction_against);
            assertNotNull("the panel still carries the direction chips", against);
            assertTrue("the panel shows the direction the canvas Flip wrote",
                    against.isActivated());
            assertFalse("and only that one", along.isActivated());
            return null;
        });

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.cadExtrudeCanvas().findViewById(R.id.cad_extrude_flip).performClick();
            return null;
        });
        settleLayout();
        assertEquals("two flips are the identity", NativeViewport.EXTRUDE_ALONG_NORMAL,
                (int) toolState()[NativeViewport.CAD_EXTRUDE_DIRECTION]);
    }

    // -----------------------------------------------------------------------
    // E2E-CADUXS1C1-03/04/05: the arrow is reachable, and dragging it works
    // -----------------------------------------------------------------------

    /**
     * The sketch's view and the extrusion's are two views, and Finish Sketch is
     * where the second begins — `CAD-UX-S1-C1`, closing `OQ-CAD-UX-01`.
     *
     * <p>This case is the one the earlier pass could not have. It asserts both
     * halves on the device: while the sketch is being AUTHORED the camera is
     * still aimed exactly along the support normal (a sketch axis stays on a
     * screen axis), and after Finish Sketch the extrusion axis has a real
     * screen projection — where before, the arrow's tip and its mid-shaft label
     * landed on the same pixel — so a pointer drag along the shaft actually
     * changes the authored depth.
     *
     * <p>Both drag pixels are asked for from native's own projection through
     * {@code cadExtrudeToolState}; no coordinate is written down here.
     */
    @Test
    public void e2eCaduxs1c1_03_finishSketchGivesAViewTheArrowCanActuallyBeDraggedIn() {
        beginSketchXy();

        // While the sketch is being AUTHORED the view is still exactly along the
        // support normal — `CAD-UX-S1-C1` changes nothing about that. Proved
        // here by the sketch's own geometry: a horizontal sketch span projects
        // to a horizontal screen span, which only an aligned view produces.
        final float[] left = new float[2];
        final float[] right = new float[2];
        assertTrue(NativeViewport.sketchScreenPoint(-1.0, 0.0, left));
        assertTrue(NativeViewport.sketchScreenPoint(1.0, 0.0, right));
        assertTrue("the sketch spans real pixels", Math.abs(right[0] - left[0]) > 40f);
        assertEquals("and the aligned view keeps a sketch axis on a screen axis",
                left[1], right[1], 2.0f);

        drawRectangle(2.0, 2.0);
        finishSketch();

        // Finish Sketch leaves that view for one the extrusion can be adjusted
        // through: the shaft now has real screen extent, where before C1 the
        // projected tip and the mid-shaft label coincided to within a pixel.
        final double[] before = toolState();
        assertEquals("the manipulator is live", 1.0,
                before[NativeViewport.CAD_EXTRUDE_ACTIVE], 0.0);
        assertTrue("and its anchor is on screen",
                before[NativeViewport.CAD_EXTRUDE_ON_SCREEN] != 0.0);
        final double shaft = Math.hypot(
                before[NativeViewport.CAD_EXTRUDE_TIP_X]
                        - before[NativeViewport.CAD_EXTRUDE_LABEL_X],
                before[NativeViewport.CAD_EXTRUDE_TIP_Y]
                        - before[NativeViewport.CAD_EXTRUDE_LABEL_Y]);
        assertTrue("CADUXS1C1-03: the extrusion axis now has a usable screen "
                        + "projection (half-shaft pixels = " + shaft + ")",
                shaft > 20.0);

        // CADUXS1C1-04: the drag itself, through real MotionEvents on the real
        // viewport, from the arrow's own projected tip along its own projected
        // direction. Both pixels come from native's projection, never written
        // down here.
        final double depthBefore = before[NativeViewport.CAD_EXTRUDE_DEPTH];
        final float tipX = (float) before[NativeViewport.CAD_EXTRUDE_TIP_X];
        final float tipY = (float) before[NativeViewport.CAD_EXTRUDE_TIP_Y];
        final float labelX = (float) before[NativeViewport.CAD_EXTRUDE_LABEL_X];
        final float labelY = (float) before[NativeViewport.CAD_EXTRUDE_LABEL_Y];
        // base -> tip on screen is +axis; the label is the mid-shaft, so
        // (tip - label) is half the shaft and doubling it is one more depth.
        final float toX = tipX + (tipX - labelX) * 2f;
        final float toY = tipY + (tipY - labelY) * 2f;
        dragViewport(tipX, tipY, toX, toY);

        final double[] after = toolState();
        assertEquals("the manipulator is still live", 1.0,
                after[NativeViewport.CAD_EXTRUDE_ACTIVE], 0.0);
        final double depthAfter = after[NativeViewport.CAD_EXTRUDE_DEPTH];
        assertFalse("never a NaN", Double.isNaN(depthAfter));
        assertTrue("still a positive length", depthAfter > 0.0);
        assertTrue("CADUXS1C1-04: the drag actually changed the depth (" + depthBefore
                        + " -> " + depthAfter + ")",
                Math.abs(depthAfter - depthBefore) > 0.05);
        assertTrue("and it grew, because the drag ran along +axis",
                depthAfter > depthBefore);
        assertEquals("the direction is untouched: a drag is not a flip",
                (int) before[NativeViewport.CAD_EXTRUDE_DIRECTION],
                (int) after[NativeViewport.CAD_EXTRUDE_DIRECTION]);
        assertEquals("CADUXS1C1-08: nothing was recorded — an uncommitted sketch "
                        + "is volatile", 0, NativeViewport.constructionUndoDepth());

        // CADUXS1C1-05: Flip and the exact value still work from this view, and
        // the exact value is what the drag left behind.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final android.widget.TextView value = workspace.cadExtrudeCanvas()
                    .findViewById(R.id.cad_extrude_depth_value);
            assertNotNull("the value chip is present", value);
            assertFalse("and it is showing something", value.getText().toString().isEmpty());
            workspace.cadExtrudeCanvas().findViewById(R.id.cad_extrude_flip).performClick();
            return null;
        });
        settleLayout();
        final double[] flipped = toolState();
        assertEquals("Flip works from the preview view",
                NativeViewport.EXTRUDE_AGAINST_NORMAL,
                (int) flipped[NativeViewport.CAD_EXTRUDE_DIRECTION]);
        assertEquals("and Flip is a DIRECTION, never a negative depth", depthAfter,
                flipped[NativeViewport.CAD_EXTRUDE_DEPTH], 1e-9);
        assertTrue("which is still positive", flipped[NativeViewport.CAD_EXTRUDE_DEPTH] > 0.0);
    }

    // -----------------------------------------------------------------------
    // E2E-CADUXS1C1-06: back to the sketch is back to the EXACT view
    // -----------------------------------------------------------------------

    @Test
    public void e2eCaduxs1c1_06_theSketchCameraReturnsAndTheNextPreviewIsUsableAgain() {
        beginSketchXy();
        drawRectangle(2.0, 1.0);
        finishSketch();

        final double previewShaft = shaftPixels();
        assertTrue("the preview view sees the axis", previewShaft > 20.0);

        // Back to Editing: the authored sketch is drawn through the aligned
        // view again, and the axis collapses to a point exactly as it should.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.back_to_sketch).performClick();
            return null;
        });
        settleLayout();
        assertEquals("the sketch is editable again", NativeViewport.SKETCH_EDITING,
                sketchState());
        final float[] a = new float[2];
        final float[] b = new float[2];
        assertTrue(NativeViewport.sketchScreenPoint(-1.0, 0.0, a));
        assertTrue(NativeViewport.sketchScreenPoint(1.0, 0.0, b));
        assertEquals("CADUXS1C1-06: the exact support-normal view is back",
                a[1], b[1], 2.0f);
        assertTrue("with the sketch at a real size", Math.abs(b[0] - a[0]) > 40f);
        assertEquals("and nothing was recorded by changing view mode", 0,
                NativeViewport.constructionUndoDepth());

        // Finish again: the preview is usable again, every time.
        finishSketch();
        assertTrue("the preview is usable on the second pass too", shaftPixels() > 20.0);
    }

    /**
     * Half the extrusion arrow's shaft, in screen pixels: the distance between
     * the projected tip and the projected mid-shaft label anchor. Zero from the
     * aligned sketch view, and a real length from the feature preview.
     */
    private double shaftPixels() {
        final double[] state = toolState();
        assertEquals("the manipulator is live", 1.0,
                state[NativeViewport.CAD_EXTRUDE_ACTIVE], 0.0);
        assertTrue("and on screen", state[NativeViewport.CAD_EXTRUDE_ON_SCREEN] != 0.0);
        return Math.hypot(
                state[NativeViewport.CAD_EXTRUDE_TIP_X]
                        - state[NativeViewport.CAD_EXTRUDE_LABEL_X],
                state[NativeViewport.CAD_EXTRUDE_TIP_Y]
                        - state[NativeViewport.CAD_EXTRUDE_LABEL_Y]);
    }

    // -----------------------------------------------------------------------
    // E2E-CADUXS1-04: the exact value at the arrow
    // -----------------------------------------------------------------------

    @Test
    public void e2eCaduxs1_04_anExactDepthTypedAtTheArrowIsTheAuthoredDepth() {
        beginSketchXy();
        drawRectangle(2.0, 1.0);
        finishSketch();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.cadExtrudeCanvas().findViewById(R.id.cad_extrude_depth_value)
                    .performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("tapping the value opens the exact field",
                    workspace.cadExtrudeCanvas().editorOpen());
            return null;
        });

        typeCanvasDepth("2.5");
        assertEquals("E2E-CADUXS1-04: the typed value is the authored depth", 2.5,
                toolState()[NativeViewport.CAD_EXTRUDE_DEPTH], 1e-9);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertFalse("and the field closes", workspace.cadExtrudeCanvas().editorOpen());
            return null;
        });

        // A typed value is REFUSED, never clamped: zero is not a length, and the
        // previous authored depth stands.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.cadExtrudeCanvas().findViewById(R.id.cad_extrude_depth_value)
                    .performClick();
            return null;
        });
        settleLayout();
        typeCanvasDepth("0");
        assertEquals("a typed zero is refused and the depth stands", 2.5,
                toolState()[NativeViewport.CAD_EXTRUDE_DEPTH], 1e-9);

        // The same value typed into the PRECISION panel is the same authored
        // depth: two routes, one truth.
        setDepthThroughPanel("2.5");
        assertEquals("the panel and the canvas reach the same authored depth", 2.5,
                toolState()[NativeViewport.CAD_EXTRUDE_DEPTH], 1e-9);
    }

    // -----------------------------------------------------------------------
    // E2E-CADUXS1-05: staged Cancel
    // -----------------------------------------------------------------------

    @Test
    public void e2eCaduxs1_05_dragFlipAndTypeThenCancelLeavesTheProjectUntouched() {
        final byte[] bytesBefore = NativeViewport.encodeProject();
        final long fingerprintBefore = NativeViewport.projectFingerprint();
        final int bodiesBefore = NativeViewport.sceneBodyCount();
        final int undoBefore = NativeViewport.constructionUndoDepth();

        beginSketchXy();
        drawRectangle(2.0, 1.0);
        finishSketch();

        final double[] state = toolState();
        dragViewport((float) state[NativeViewport.CAD_EXTRUDE_TIP_X],
                (float) state[NativeViewport.CAD_EXTRUDE_TIP_Y],
                (float) state[NativeViewport.CAD_EXTRUDE_TIP_X],
                (float) state[NativeViewport.CAD_EXTRUDE_TIP_Y] - 180f);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.cadExtrudeCanvas().findViewById(R.id.cad_extrude_flip).performClick();
            workspace.cadExtrudeCanvas().findViewById(R.id.cad_extrude_depth_value)
                    .performClick();
            return null;
        });
        settleLayout();
        typeCanvasDepth("3.25");

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.cancel_sketch).performClick();
            return null;
        });
        settleLayout();

        assertEquals("E2E-CADUXS1-05: no body was created", bodiesBefore,
                NativeViewport.sceneBodyCount());
        assertEquals("nothing was recorded", undoBefore, NativeViewport.constructionUndoDepth());
        assertEquals("the fingerprint did not move", fingerprintBefore,
                NativeViewport.projectFingerprint());
        assertArrayEquals("and the document is byte-identical", bytesBefore,
                NativeViewport.encodeProject());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("the whole canvas surface is withdrawn with the sketch", View.GONE,
                    workspace.cadExtrudeCanvas().getVisibility());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // E2E-CADUXS1-06/07: Apply is the existing New Body path, and one Undo
    // -----------------------------------------------------------------------

    @Test
    public void e2eCaduxs1_06and07_applyIsOneTransactionAndTheRetainedSketchIsOneTapAway() {
        final int bodiesBefore = NativeViewport.sceneBodyCount();
        final int undoBefore = NativeViewport.constructionUndoDepth();

        beginSketchXy();
        drawRectangle(2.0, 1.0);
        finishSketch();
        typeCanvasDepthFromChip("1.5");

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.extrude_sketch).performClick();
            return null;
        });
        settleLayout();

        assertEquals("the sketch closed", NativeViewport.SKETCH_INACTIVE, sketchState());
        assertEquals("E2E-CADUXS1-06: exactly one new body", bodiesBefore + 1,
                NativeViewport.sceneBodyCount());
        assertEquals("as exactly one history step", undoBefore + 1,
                NativeViewport.constructionUndoDepth());
        assertTrue("and it is a CAD body", NativeViewport.sceneActiveBodyIsCad());
        final long body = NativeViewport.sceneActiveBodyId();
        final double[] cad = new double[NativeViewport.CAD_STATE_SIZE];
        assertTrue(NativeViewport.cadState(cad));
        assertEquals("extruded to the depth that was typed at the arrow", 1.5,
                cad[NativeViewport.CAD_DEPTH], 1e-9);

        // E2E-CADUXS1-07: the retained sketch. The manipulator is gone with the
        // session; what stands on the body is the chip that reopens its sketch.
        assertEquals("the manipulator is gone with the session", 0.0,
                toolState()[NativeViewport.CAD_EXTRUDE_ACTIVE], 0.0);
        final float[] anchor = new float[3];
        assertTrue("the committed body's sketch still has an anchor",
                NativeViewport.cadBodySketchAnchor(body, anchor));

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final CadExtrudeCanvasView canvas = workspace.cadExtrudeCanvas();
            assertEquals("E2E-CADUXS1-07: the canvas offers Edit Sketch", View.VISIBLE,
                    canvas.findViewById(R.id.cad_canvas_edit_sketch).getVisibility());
            assertEquals("standing on this body", body, canvas.retainedSketchBodyId());
            return null;
        });

        // ONE tap reopens the retained sketch, staged.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.cadExtrudeCanvas().findViewById(R.id.cad_canvas_edit_sketch)
                    .performClick();
            return null;
        });
        settleLayout();
        assertEquals("a staged edit of the same body", body, NativeViewport.sketchEditingBodyId());
        assertEquals("with the sketch open", NativeViewport.SKETCH_EDITING, sketchState());
        assertEquals("and nothing recorded by opening it", undoBefore + 1,
                NativeViewport.constructionUndoDepth());

        // Change one authored value and finish: the SAME body updates, as one
        // more step, and no second body appears.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.finish_sketch).performClick();
            return null;
        });
        settleLayout();
        setDepthThroughPanel("2.25");
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.extrude_sketch).performClick();
            return null;
        });
        settleLayout();

        assertEquals("E2E-CADUXS1-07: the same body was updated", body,
                NativeViewport.sceneActiveBodyId());
        assertEquals("no second body was created", bodiesBefore + 1,
                NativeViewport.sceneBodyCount());
        assertEquals("as one more step", undoBefore + 2, NativeViewport.constructionUndoDepth());
        assertTrue(NativeViewport.cadState(cad));
        assertEquals("carrying the edited depth", 2.25, cad[NativeViewport.CAD_DEPTH], 1e-9);
    }

    // -----------------------------------------------------------------------
    // E2E-CADUXS1-08: no false affordance
    // -----------------------------------------------------------------------

    @Test
    public void e2eCaduxs1_08_addCutSymmetricAndTwoSidesAreNowhereToBeFound() {
        beginSketchXy();
        drawRectangle(2.0, 1.0);
        finishSketch();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View canvas = workspace.cadExtrudeCanvas();
            // The badge STATES the operation; there is nothing to choose, so
            // nothing that could not succeed is drawn.
            final String badge = ((android.widget.TextView)
                    canvas.findViewById(R.id.cad_extrude_operation)).getText().toString();
            assertEquals("only New Body is named",
                    activity.getString(R.string.extrude_operation_new_body), badge);
            for (String forbidden : new String[]{"Add", "Cut", "Symmetric", "Two Sides",
                    "Asymmetric", "Boolean", "Union", "Subtract"}) {
                assertFalse("E2E-CADUXS1-08: the cluster never names " + forbidden,
                        containsText(canvas, forbidden));
                assertFalse("nor does the sketch panel", containsText(workspace.sketchEditor(),
                        forbidden));
            }
            return null;
        });

        // And the domain cannot express one either: the direction is two-valued.
        final double[] state = toolState();
        final int direction = (int) state[NativeViewport.CAD_EXTRUDE_DIRECTION];
        assertTrue("the extrusion is one of exactly two directions",
                direction == NativeViewport.EXTRUDE_ALONG_NORMAL
                        || direction == NativeViewport.EXTRUDE_AGAINST_NORMAL);
    }

    // -----------------------------------------------------------------------
    // E2E-CADUXS1-09: camera-attached presentation
    // -----------------------------------------------------------------------

    @Test
    public void e2eCaduxs1_09_theClusterIsCameraAttachedAndBounded() {
        beginSketchXy();
        drawRectangle(2.0, 1.0);
        finishSketch();

        final double[] near = toolState();
        assertTrue("the scale is inside the bounded band",
                near[NativeViewport.CAD_EXTRUDE_SCALE] >= 0.80 - 1e-6
                        && near[NativeViewport.CAD_EXTRUDE_SCALE] <= 1.60 + 1e-6);

        // Pull the camera back with a real two-finger pinch and read the scale
        // again. The presentation follows the camera; the AUTHORED depth does
        // not, which is the whole point of the rule.
        final double depthBefore = near[NativeViewport.CAD_EXTRUDE_DEPTH];
        pinchOut();
        final double[] far = toolState();
        assertEquals("E2E-CADUXS1-09: zooming changes no authored value", depthBefore,
                far[NativeViewport.CAD_EXTRUDE_DEPTH], 0.0);
        assertTrue("and the scale stays inside the band",
                far[NativeViewport.CAD_EXTRUDE_SCALE] >= 0.80 - 1e-6
                        && far[NativeViewport.CAD_EXTRUDE_SCALE] <= 1.60 + 1e-6);
        assertTrue("a farther camera never draws the cluster larger",
                far[NativeViewport.CAD_EXTRUDE_SCALE] <= near[NativeViewport.CAD_EXTRUDE_SCALE]
                        + 1e-6);
        assertTrue("and the clamp code is one of the three it may be",
                far[NativeViewport.CAD_EXTRUDE_CLAMP] == NativeViewport.CAD_EXTRUDE_CLAMP_NONE
                        || far[NativeViewport.CAD_EXTRUDE_CLAMP]
                                == NativeViewport.CAD_EXTRUDE_CLAMP_LOW
                        || far[NativeViewport.CAD_EXTRUDE_CLAMP]
                                == NativeViewport.CAD_EXTRUDE_CLAMP_HIGH);
    }

    // -----------------------------------------------------------------------
    // Helpers. Every pixel is asked for from native; none is written down.
    // -----------------------------------------------------------------------

    private void beginSketchXy() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openAddPrimitive(workspace);
            workspace.addPrimitivePalette().findViewById(R.id.sketch_plane_by_name).performClick();
            workspace.addPrimitivePalette().findViewById(R.id.sketch_plane_xy).performClick();
            return null;
        });
        settleLayout();
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
    }

    private void drawRectangle(double width, double height) {
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), -width / 2, -height / 2, width / 2, height / 2);
    }

    private void finishSketch() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.finish_sketch).performClick();
            return null;
        });
        settleLayout();
        assertEquals(NativeViewport.SKETCH_READY, sketchState());
    }

    private double[] toolState() {
        final double[] state = new double[NativeViewport.CAD_EXTRUDE_SIZE];
        NativeViewport.cadExtrudeToolState(state);
        return state;
    }

    /** Types a depth into the PRECISION panel and submits it, for the parity cases. */
    private void setDepthThroughPanel(final String depth) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final EditText field = workspace.sketchEditor().findViewById(R.id.field_extrude_depth);
            field.setText(depth);
            workspace.sketchEditor().findViewById(R.id.extrude_direction_along).performClick();
            return null;
        });
        settleLayout();
    }

    /** Opens the canvas field, types, and applies. */
    private void typeCanvasDepthFromChip(final String depth) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.cadExtrudeCanvas().findViewById(R.id.cad_extrude_depth_value)
                    .performClick();
            return null;
        });
        settleLayout();
        typeCanvasDepth(depth);
    }

    private void typeCanvasDepth(final String depth) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final EditText field = workspace.cadExtrudeCanvas()
                    .findViewById(R.id.field_cad_extrude_depth);
            assertNotNull("the canvas field is open", field);
            field.setText(depth);
            workspace.cadExtrudeCanvas().findViewById(R.id.apply_cad_extrude_depth)
                    .performClick();
            return null;
        });
        settleLayout();
    }

    /** A one-finger drag across the real viewport, through real MotionEvents. */
    private void dragViewport(final float fromX, final float fromY, final float toX,
                              final float toY) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final long down = SystemClock.uptimeMillis();
            send(viewport, down, down, MotionEvent.ACTION_DOWN, fromX, fromY);
            for (int step = 1; step <= 8; ++step) {
                final float t = step / 8f;
                send(viewport, down, down + step * 12L, MotionEvent.ACTION_MOVE,
                        fromX + (toX - fromX) * t, fromY + (toY - fromY) * t);
            }
            send(viewport, down, down + 128L, MotionEvent.ACTION_UP, toX, toY);
            return null;
        });
        settleLayout();
    }

    /** A two-finger pinch INWARD, which pulls the camera back. */
    private void pinchOut() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final float cx = viewport.getWidth() * 0.5f;
            final float cy = viewport.getHeight() * 0.5f;
            final long down = SystemClock.uptimeMillis();
            send(viewport, down, down, MotionEvent.ACTION_DOWN, cx - 300f, cy);
            sendTwo(viewport, down, down + 8L,
                    MotionEvent.ACTION_POINTER_DOWN | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT),
                    cx - 300f, cy, cx + 300f, cy);
            for (int step = 1; step <= 8; ++step) {
                final float shrink = 300f - 30f * step;
                sendTwo(viewport, down, down + 8L + step * 12L, MotionEvent.ACTION_MOVE,
                        cx - shrink, cy, cx + shrink, cy);
            }
            sendTwo(viewport, down, down + 140L,
                    MotionEvent.ACTION_POINTER_UP | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT),
                    cx - 60f, cy, cx + 60f, cy);
            send(viewport, down, down + 150L, MotionEvent.ACTION_UP, cx - 60f, cy);
            return null;
        });
        settleLayout();
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

    private static void sendTwo(View target, long downTime, long eventTime, int action, float x0,
                                float y0, float x1, float y1) {
        final MotionEvent.PointerProperties[] props = new MotionEvent.PointerProperties[2];
        final MotionEvent.PointerCoords[] coords = new MotionEvent.PointerCoords[2];
        for (int i = 0; i < 2; ++i) {
            props[i] = new MotionEvent.PointerProperties();
            props[i].id = i;
            props[i].toolType = MotionEvent.TOOL_TYPE_FINGER;
            coords[i] = new MotionEvent.PointerCoords();
        }
        coords[0].x = x0;
        coords[0].y = y0;
        coords[1].x = x1;
        coords[1].y = y1;
        final MotionEvent event = MotionEvent.obtain(downTime, eventTime, action, 2, props, coords,
                0, 0, 1f, 1f, 0, 0, android.view.InputDevice.SOURCE_TOUCHSCREEN, 0);
        try {
            target.dispatchTouchEvent(event);
        } finally {
            event.recycle();
        }
    }

    /** Whether any TextView under `root` shows `text`. Case-insensitive. */
    private static boolean containsText(View root, String text) {
        if (root instanceof android.widget.TextView) {
            final CharSequence shown = ((android.widget.TextView) root).getText();
            if (shown != null && shown.toString().toLowerCase().contains(text.toLowerCase())) {
                return true;
            }
        }
        if (root instanceof android.view.ViewGroup) {
            final android.view.ViewGroup group = (android.view.ViewGroup) root;
            for (int i = 0; i < group.getChildCount(); ++i) {
                if (containsText(group.getChildAt(i), text)) {
                    return true;
                }
            }
        }
        return false;
    }
}
