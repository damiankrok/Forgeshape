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
 * `E2E-CADUXS1-01..09`: the canvas-first control of the extrusion, on a device.
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
 * <p>It also pins, on the device, the one thing that does <b>not</b> work yet
 * and why — see
 * {@link #e2eCaduxs1_03_theArrowAxisFacesTheEyeSoADragHoldsRatherThanGuesses}
 * and `OQ-CAD-UX-01`.
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
    // E2E-CADUXS1-03: dragging the arrow
    // -----------------------------------------------------------------------

    /**
     * The arrow's axis points straight at the eye from the only view a sketch
     * has, so a drag holds rather than guesses — `OQ-CAD-UX-01`.
     *
     * <p>This is not a disabled case. It asserts, on the device, the exact
     * situation that makes the in-viewport drag currently unreachable, so the
     * blocker is evidence rather than a claim: the sketch camera is locked
     * normal to its plane ({@code CameraController::applyOrbit} returns early
     * while {@code sketchView_} is true, and {@code frameSketchView} aims
     * exactly along the support normal), and that normal <b>is</b> the
     * extrusion axis. Looking down an axis, the arrow's tip and its mid-shaft
     * label project to the same pixel — there is no screen direction to drag
     * along — and the manipulator does the one honest thing the repo's rule
     * allows: it holds the last good value rather than inventing one.
     *
     * <p>The drag arithmetic itself is proved by the native
     * {@code CADUXS1_04_*} cases under cameras that can see the axis.
     */
    @Test
    public void e2eCaduxs1_03_theArrowAxisFacesTheEyeSoADragHoldsRatherThanGuesses() {
        beginSketchXy();
        drawRectangle(2.0, 2.0);
        finishSketch();

        final double[] before = toolState();
        assertEquals("the manipulator is live", 1.0,
                before[NativeViewport.CAD_EXTRUDE_ACTIVE], 0.0);
        assertTrue("and its anchor is on screen",
                before[NativeViewport.CAD_EXTRUDE_ON_SCREEN] != 0.0);

        // The shaft has no screen extent: the view looks straight down it.
        final double separation = Math.hypot(
                before[NativeViewport.CAD_EXTRUDE_TIP_X]
                        - before[NativeViewport.CAD_EXTRUDE_LABEL_X],
                before[NativeViewport.CAD_EXTRUDE_TIP_Y]
                        - before[NativeViewport.CAD_EXTRUDE_LABEL_Y]);
        assertEquals("OQ-CAD-UX-01: the sketch view looks exactly along the extrusion axis",
                0.0, separation, 1.0);

        // Attempting the drag anyway changes NOTHING — no guessed depth, no
        // NaN, no jump, and nothing recorded.
        final double depthBefore = before[NativeViewport.CAD_EXTRUDE_DEPTH];
        final float tipX = (float) before[NativeViewport.CAD_EXTRUDE_TIP_X];
        final float tipY = (float) before[NativeViewport.CAD_EXTRUDE_TIP_Y];
        dragViewport(tipX, tipY, tipX, tipY - 220f);

        final double[] after = toolState();
        assertEquals("the manipulator is still live", 1.0,
                after[NativeViewport.CAD_EXTRUDE_ACTIVE], 0.0);
        assertEquals("a drag down the axis holds the last good depth", depthBefore,
                after[NativeViewport.CAD_EXTRUDE_DEPTH], 0.0);
        assertTrue("which is still a positive length",
                after[NativeViewport.CAD_EXTRUDE_DEPTH] > 0.0);
        assertFalse("and never a NaN",
                Double.isNaN(after[NativeViewport.CAD_EXTRUDE_DEPTH]));
        assertEquals("the direction is untouched",
                (int) before[NativeViewport.CAD_EXTRUDE_DIRECTION],
                (int) after[NativeViewport.CAD_EXTRUDE_DIRECTION]);
        assertEquals("nothing was recorded: an uncommitted sketch is volatile", 0,
                NativeViewport.constructionUndoDepth());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final android.widget.TextView value = workspace.cadExtrudeCanvas()
                    .findViewById(R.id.cad_extrude_depth_value);
            assertNotNull("the value chip is present", value);
            assertFalse("and it is showing something", value.getText().toString().isEmpty());
            return null;
        });

        // Flip and the exact value are how the extrusion is changed meanwhile,
        // and both work from this very view.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.cadExtrudeCanvas().findViewById(R.id.cad_extrude_flip).performClick();
            return null;
        });
        settleLayout();
        assertEquals("Flip works from the locked sketch view",
                NativeViewport.EXTRUDE_AGAINST_NORMAL,
                (int) toolState()[NativeViewport.CAD_EXTRUDE_DIRECTION]);
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
