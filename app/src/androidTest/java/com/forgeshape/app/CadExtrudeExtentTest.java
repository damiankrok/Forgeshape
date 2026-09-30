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
import static org.junit.Assert.assertNull;
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
 * `CADEXT-06/07/12`: One Side, Symmetric and Two Sides on a device.
 *
 * <h2>What this suite is for</h2>
 *
 * <p>The DOMAIN half — the two distances, the mesh offsets, every transition of
 * the mode policy, the canonical form, the refusals, the history and the whole
 * `CADB` v4 codec including its six golden fixtures — is proved by the native
 * `CADEXT_*` cases in the CAD self-test, which build their own scenes, their own
 * histories and their own cameras. What is left, and what this covers, is
 * everything that can only be true on a device: that the extent selector is
 * really there and really changes the mode, that Symmetric shows ONE distance
 * for two arrows while Two Sides shows two, that a real MotionEvent drag on
 * either arrow moves the side it was started on, that Flip is ABSENT where
 * there is no side to choose, that the retained sketch comes back carrying the
 * extent it was committed with, and that a Save/Open round trip gives the exact
 * extent back.
 *
 * <h2>Rules</h2>
 *
 * <p>No control is located by coordinate. The one place a pixel appears is a
 * viewport gesture, and every such pixel is asked for from native's own
 * projection through {@code cadExtrudeToolState}, never written down.
 */
@RunWith(AndroidJUnit4.class)
public final class CadExtrudeExtentTest {

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
    // CADEXT-06 a: the selector is drawn, and it really changes the mode
    // -----------------------------------------------------------------------

    @Test
    public void cadext06a_theExtentSelectorIsDrawnAndChangesTheMode() {
        beginSketchXy();
        drawRectangle(2.0, 1.0);
        finishSketch();

        double[] state = toolState();
        assertEquals("a fresh extrusion is One Side", NativeViewport.EXTENT_ONE_SIDE,
                (int) state[NativeViewport.CAD_EXTRUDE_EXTENT]);
        final double depth = state[NativeViewport.CAD_EXTRUDE_DEPTH];
        assertEquals("all of it on the +N side", depth,
                state[NativeViewport.CAD_EXTRUDE_POSITIVE], 0.0);
        assertEquals("and none on the other", 0.0,
                state[NativeViewport.CAD_EXTRUDE_NEGATIVE], 0.0);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View canvas = workspace.cadExtrudeCanvas();
            assertEquals("the cluster is drawn", View.VISIBLE, canvas.getVisibility());
            for (int id : new int[]{R.id.cad_extrude_extent_one_side,
                    R.id.cad_extrude_extent_symmetric, R.id.cad_extrude_extent_two_sides}) {
                final View chip = canvas.findViewById(id);
                assertNotNull("the selector carries every extent", chip);
                assertEquals("and offers it", View.VISIBLE, chip.getVisibility());
            }
            assertTrue("One Side reads as the current one",
                    canvas.findViewById(R.id.cad_extrude_extent_one_side).isActivated());
            assertEquals("and Flip is offered, because there is a side to choose", View.VISIBLE,
                    canvas.findViewById(R.id.cad_extrude_flip).getVisibility());
            // The 48 dp interactive floor holds for the new controls too. Since
            // `CAD-FOUNDATION-C2` the three choices stand in the action palette
            // the ONE panel opens, so the panel's proxy is the target drawn at
            // rest; the choices' own floor is asserted with the palette open by
            // CadVerticalSliceTest.compact_extrude_hud.
            final float density = activity.getResources().getDisplayMetrics().density;
            final int floor = Math.round(48f * density);
            final View panel = canvas.findViewById(R.id.cad_extrude_panel);
            assertTrue("the panel is a real target",
                    panel.getHeight() >= floor - 1 && panel.getWidth() >= floor - 1);
            return null;
        });

        chooseExtent(R.id.cad_extrude_extent_symmetric);
        state = toolState();
        assertEquals("CADEXT-06: the selector changed the mode",
                NativeViewport.EXTENT_SYMMETRIC,
                (int) state[NativeViewport.CAD_EXTRUDE_EXTENT]);
        assertEquals("carrying the depth onto the +N side", depth,
                state[NativeViewport.CAD_EXTRUDE_POSITIVE], 1e-9);
        assertEquals("and equally onto the -N side", depth,
                state[NativeViewport.CAD_EXTRUDE_NEGATIVE], 1e-9);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View canvas = workspace.cadExtrudeCanvas();
            assertTrue("Symmetric reads as the current one",
                    canvas.findViewById(R.id.cad_extrude_extent_symmetric).isActivated());
            assertFalse("and only that one",
                    canvas.findViewById(R.id.cad_extrude_extent_one_side).isActivated());
            assertEquals("Flip is ABSENT: both sides are reached already", View.GONE,
                    canvas.findViewById(R.id.cad_extrude_flip).getVisibility());
            assertFalse("and Symmetric states ONE distance for two arrows",
                    canvas.findViewById(R.id.cad_extrude_second_value).isShown());
            // The panel's side chips go with it, on the same terms.
            workspace.sketchEditor().refreshFromNative();
            assertFalse("nor does the panel offer a side",
                    workspace.sketchEditor().findViewById(R.id.extrude_direction_along)
                            .isShown());
            return null;
        });

        chooseExtent(R.id.cad_extrude_extent_two_sides);
        state = toolState();
        assertEquals("Two Sides", NativeViewport.EXTENT_TWO_SIDES,
                (int) state[NativeViewport.CAD_EXTRUDE_EXTENT]);
        assertEquals("A carries across", depth, state[NativeViewport.CAD_EXTRUDE_POSITIVE], 1e-9);
        assertEquals("and so does B", depth, state[NativeViewport.CAD_EXTRUDE_NEGATIVE], 1e-9);
        final double[] twoSides = state;
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View canvas = workspace.cadExtrudeCanvas();
            assertTrue("Two Sides states BOTH distances",
                    canvas.findViewById(R.id.cad_extrude_second_value).isShown());
            assertEquals("and still offers no Flip", View.GONE,
                    canvas.findViewById(R.id.cad_extrude_flip).getVisibility());
            // `CAD-FOUNDATION-C1` E7: each value stands above its OWN side's
            // leader, so neither number is ambiguous about which side it states.
            final float density = activity.getResources().getDisplayMetrics().density;
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final String a = CadLeaderHudChecks.valueOnLeader(twoSides,
                    canvas.findViewById(R.id.cad_extrude_depth_value), viewport, density, false);
            final String b = CadLeaderHudChecks.valueOnLeader(twoSides,
                    canvas.findViewById(R.id.cad_extrude_second_value), viewport, density, true);
            assertNull("Side A on the +N leader: " + a, a);
            assertNull("Side B on the -N leader: " + b, b);
            return null;
        });

        // Back to One Side: the mode round trip lands on a positive depth on a
        // real side, never a negative one.
        chooseExtent(R.id.cad_extrude_extent_one_side);
        state = toolState();
        assertEquals(NativeViewport.EXTENT_ONE_SIDE,
                (int) state[NativeViewport.CAD_EXTRUDE_EXTENT]);
        assertTrue("the depth stayed positive",
                state[NativeViewport.CAD_EXTRUDE_DEPTH] > 0.0);
        assertEquals("and only one side has extent", 0.0,
                state[NativeViewport.CAD_EXTRUDE_NEGATIVE], 0.0);
    }

    // -----------------------------------------------------------------------
    // CADEXT-06 b: exact values, one field per side that has one
    // -----------------------------------------------------------------------

    @Test
    public void cadext06b_exactValuesLandOnTheSideTheyBelongTo() {
        beginSketchXy();
        drawRectangle(2.0, 1.0);
        finishSketch();

        // Symmetric: ONE field, and it writes BOTH sides.
        chooseExtent(R.id.cad_extrude_extent_symmetric);
        typeFirst("0.75");
        double[] state = toolState();
        assertEquals("CADEXT-06: the Symmetric field is each side", 0.75,
                state[NativeViewport.CAD_EXTRUDE_POSITIVE], 1e-9);
        assertEquals("on both of them", 0.75,
                state[NativeViewport.CAD_EXTRUDE_NEGATIVE], 1e-9);

        // Two Sides: two fields, and each moves only its own side.
        chooseExtent(R.id.cad_extrude_extent_two_sides);
        typeFirst("1.25");
        state = toolState();
        assertEquals("A took the typed value", 1.25,
                state[NativeViewport.CAD_EXTRUDE_POSITIVE], 1e-9);
        assertEquals("and B did not move", 0.75,
                state[NativeViewport.CAD_EXTRUDE_NEGATIVE], 1e-9);

        typeSecond("0.5");
        state = toolState();
        assertEquals("B took the typed value", 0.5,
                state[NativeViewport.CAD_EXTRUDE_NEGATIVE], 1e-9);
        assertEquals("and A did not move", 1.25,
                state[NativeViewport.CAD_EXTRUDE_POSITIVE], 1e-9);

        // A refused value moves nothing. Refused, never clamped.
        typeSecond("-1");
        state = toolState();
        assertEquals("a refused distance leaves B alone", 0.5,
                state[NativeViewport.CAD_EXTRUDE_NEGATIVE], 1e-9);
        assertEquals("and A alone", 1.25, state[NativeViewport.CAD_EXTRUDE_POSITIVE], 1e-9);

        // A side of ZERO is legitimate in Two Sides — the other side carries the
        // extent — and the value stays readable and typeable back up, standing
        // at the base where its arrow has no length to have been drawn.
        typeSecond("0");
        state = toolState();
        assertEquals("a zero side is accepted where the other carries the extent", 0.0,
                state[NativeViewport.CAD_EXTRUDE_NEGATIVE], 0.0);
        assertEquals("still Two Sides", NativeViewport.EXTENT_TWO_SIDES,
                (int) state[NativeViewport.CAD_EXTRUDE_EXTENT]);
        assertTrue("and its value still has somewhere honest to stand",
                state[NativeViewport.CAD_EXTRUDE_SECOND_ON_SCREEN] != 0.0);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("so the second field is still reachable",
                    workspace.cadExtrudeCanvas().findViewById(R.id.cad_extrude_second_value)
                            .isShown());
            return null;
        });
        typeSecond("0.375");
        assertEquals("and typing it back up works", 0.375,
                toolState()[NativeViewport.CAD_EXTRUDE_NEGATIVE], 1e-9);
    }

    // -----------------------------------------------------------------------
    // CADEXT-06 c: a real drag, on the side it was started on
    // -----------------------------------------------------------------------

    @Test
    public void cadext06c_draggingEitherArrowMovesTheSideItStartedOn() {
        beginSketchXy();
        drawRectangle(2.0, 2.0);
        finishSketch();
        chooseExtent(R.id.cad_extrude_extent_two_sides);
        typeFirst("1.0");
        typeSecond("1.0");

        double[] state = toolState();
        assertTrue("both anchors project on screen",
                state[NativeViewport.CAD_EXTRUDE_ON_SCREEN] != 0.0
                        && state[NativeViewport.CAD_EXTRUDE_SECOND_ON_SCREEN] != 0.0);
        assertTrue("and the two arrows are drawn apart",
                Math.hypot(state[NativeViewport.CAD_EXTRUDE_TIP_X]
                                   - state[NativeViewport.CAD_EXTRUDE_SECOND_TIP_X],
                           state[NativeViewport.CAD_EXTRUDE_TIP_Y]
                                   - state[NativeViewport.CAD_EXTRUDE_SECOND_TIP_Y]) > 24.0);

        // Drag the PRIMARY arrow along its own shaft, from its label to past its
        // tip. Every pixel comes from native's projection.
        final double aBefore = state[NativeViewport.CAD_EXTRUDE_POSITIVE];
        final double bBefore = state[NativeViewport.CAD_EXTRUDE_NEGATIVE];
        dragAlong(state[NativeViewport.CAD_EXTRUDE_LABEL_X],
                  state[NativeViewport.CAD_EXTRUDE_LABEL_Y],
                  state[NativeViewport.CAD_EXTRUDE_TIP_X],
                  state[NativeViewport.CAD_EXTRUDE_TIP_Y]);
        state = toolState();
        assertTrue("CADEXT-06: dragging the +N arrow moved A",
                Math.abs(state[NativeViewport.CAD_EXTRUDE_POSITIVE] - aBefore) > 1e-4);
        assertEquals("and left B exactly alone", bBefore,
                state[NativeViewport.CAD_EXTRUDE_NEGATIVE], 0.0);
        assertTrue("A is still positive", state[NativeViewport.CAD_EXTRUDE_POSITIVE] > 0.0);
        assertEquals("and the mode did not change", NativeViewport.EXTENT_TWO_SIDES,
                (int) state[NativeViewport.CAD_EXTRUDE_EXTENT]);

        // Now the SECOND arrow, the same way.
        final double aHeld = state[NativeViewport.CAD_EXTRUDE_POSITIVE];
        dragAlong(state[NativeViewport.CAD_EXTRUDE_SECOND_LABEL_X],
                  state[NativeViewport.CAD_EXTRUDE_SECOND_LABEL_Y],
                  state[NativeViewport.CAD_EXTRUDE_SECOND_TIP_X],
                  state[NativeViewport.CAD_EXTRUDE_SECOND_TIP_Y]);
        state = toolState();
        assertTrue("dragging the -N arrow moved B",
                Math.abs(state[NativeViewport.CAD_EXTRUDE_NEGATIVE] - bBefore) > 1e-4);
        assertEquals("and left A exactly alone", aHeld,
                state[NativeViewport.CAD_EXTRUDE_POSITIVE], 0.0);
        assertTrue("B is still positive", state[NativeViewport.CAD_EXTRUDE_NEGATIVE] > 0.0);

        // Symmetric: either arrow writes the ONE distance, so the two stay equal
        // through a drag as well as through a typed value.
        chooseExtent(R.id.cad_extrude_extent_symmetric);
        state = toolState();
        dragAlong(state[NativeViewport.CAD_EXTRUDE_LABEL_X],
                  state[NativeViewport.CAD_EXTRUDE_LABEL_Y],
                  state[NativeViewport.CAD_EXTRUDE_TIP_X],
                  state[NativeViewport.CAD_EXTRUDE_TIP_Y]);
        state = toolState();
        assertEquals("CADEXT-06: a Symmetric drag keeps the two sides equal",
                state[NativeViewport.CAD_EXTRUDE_POSITIVE],
                state[NativeViewport.CAD_EXTRUDE_NEGATIVE], 0.0);
    }

    // -----------------------------------------------------------------------
    // CADEXT-07: the retained sketch comes back with its extent
    // -----------------------------------------------------------------------

    @Test
    public void cadext07_editSketchKeepsTheExtentAndIsOneTransaction() {
        final int bodiesBefore = NativeViewport.sceneBodyCount();
        final int undoBefore = NativeViewport.constructionUndoDepth();

        beginSketchXy();
        drawRectangle(2.0, 1.0);
        finishSketch();
        chooseExtent(R.id.cad_extrude_extent_two_sides);
        typeFirst("1.25");
        typeSecond("0.5");
        extrude();

        assertEquals("exactly one new body", bodiesBefore + 1, NativeViewport.sceneBodyCount());
        assertEquals("as exactly one history step", undoBefore + 1,
                NativeViewport.constructionUndoDepth());
        final long body = NativeViewport.sceneActiveBodyId();
        final double[] cad = new double[NativeViewport.CAD_STATE_SIZE];
        assertTrue(NativeViewport.cadState(cad));
        assertEquals("the committed body is Two Sides", NativeViewport.EXTENT_TWO_SIDES,
                (int) cad[NativeViewport.CAD_STATE_EXTENT]);
        assertEquals("with A as typed", 1.25, cad[NativeViewport.CAD_DEPTH], 1e-9);

        // Reopen the retained sketch: the staged copy carries the extent.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.cadExtrudeCanvas().findViewById(R.id.cad_canvas_edit_sketch).performClick();
            return null;
        });
        settleLayout();
        assertEquals("a staged edit of the same body", body, NativeViewport.sketchEditingBodyId());
        finishSketch();

        double[] state = toolState();
        assertEquals("CADEXT-07: the staged sketch came back Two Sides",
                NativeViewport.EXTENT_TWO_SIDES,
                (int) state[NativeViewport.CAD_EXTRUDE_EXTENT]);
        assertEquals("with A", 1.25, state[NativeViewport.CAD_EXTRUDE_POSITIVE], 1e-9);
        assertEquals("and B", 0.5, state[NativeViewport.CAD_EXTRUDE_NEGATIVE], 1e-9);

        // Editing the sketch GEOMETRY does not reset the extent to One Side.
        selectTool(rule.getScenario(), R.id.tool_rail_select);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.back_to_sketch).performClick();
            return null;
        });
        settleLayout();
        finishSketch();
        state = toolState();
        assertEquals("a trip back through the drawing keeps the extent",
                NativeViewport.EXTENT_TWO_SIDES,
                (int) state[NativeViewport.CAD_EXTRUDE_EXTENT]);
        assertEquals("and both distances", 0.5,
                state[NativeViewport.CAD_EXTRUDE_NEGATIVE], 1e-9);

        typeSecond("0.25");
        extrude();
        assertEquals("CADEXT-07: the SAME body was updated", body,
                NativeViewport.sceneActiveBodyId());
        assertEquals("no second body was created", bodiesBefore + 1,
                NativeViewport.sceneBodyCount());
        assertEquals("as one more step", undoBefore + 2,
                NativeViewport.constructionUndoDepth());

        // CADEXT-08 on the device: Undo restores the WHOLE previous extent.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.constructionUndo();
            return null;
        });
        settleLayout();
        assertTrue(NativeViewport.cadState(cad));
        assertEquals("Undo restored the mode", NativeViewport.EXTENT_TWO_SIDES,
                (int) cad[NativeViewport.CAD_STATE_EXTENT]);
        assertEquals("and the primary distance it had", 1.25,
                cad[NativeViewport.CAD_DEPTH], 1e-9);
    }

    // -----------------------------------------------------------------------
    // CADEXT-12: the persistence lifecycle
    // -----------------------------------------------------------------------

    @Test
    public void cadext12_saveAndOpenPreserveTheExactExtent() {
        // A Symmetric body and a Two Sides body in ONE project, so the round
        // trip carries both new modes and the legacy one beside them.
        beginSketchXy();
        drawRectangle(2.0, 1.0);
        finishSketch();
        chooseExtent(R.id.cad_extrude_extent_symmetric);
        typeFirst("0.75");
        extrude();
        final long symmetric = NativeViewport.sceneActiveBodyId();

        beginSketchXy();
        drawRectangle(1.0, 1.0);
        finishSketch();
        chooseExtent(R.id.cad_extrude_extent_two_sides);
        typeFirst("1.25");
        typeSecond("0.5");
        extrude();
        final long twoSides = NativeViewport.sceneActiveBodyId();

        final byte[] saved = NativeViewport.encodeProject();
        assertTrue("the project encodes", saved != null && saved.length > 0);
        final long fingerprint = NativeViewport.projectFingerprint();

        // Round trip through the ordinary all-or-nothing load path.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("the saved document loads", NativeViewport.PROJECT_OK,
                    NativeViewport.loadProject(saved));
            return null;
        });
        settleLayout();

        assertArrayEquals("CADEXT-12: the round trip is byte-identical", saved,
                NativeViewport.encodeProject());
        assertEquals("and the fingerprint came back", fingerprint,
                NativeViewport.projectFingerprint());

        final double[] cad = new double[NativeViewport.CAD_STATE_SIZE];
        selectBody(symmetric);
        assertTrue(NativeViewport.cadState(cad));
        assertEquals("the Symmetric body reopened Symmetric", NativeViewport.EXTENT_SYMMETRIC,
                (int) cad[NativeViewport.CAD_STATE_EXTENT]);
        assertEquals("with its per-side distance", 0.75, cad[NativeViewport.CAD_DEPTH], 1e-9);

        selectBody(twoSides);
        assertTrue(NativeViewport.cadState(cad));
        assertEquals("and the Two Sides body reopened Two Sides", NativeViewport.EXTENT_TWO_SIDES,
                (int) cad[NativeViewport.CAD_STATE_EXTENT]);
        assertEquals("with A", 1.25, cad[NativeViewport.CAD_DEPTH], 1e-9);

        // The session camera is not project truth: reopening returns to the
        // ordinary editor with no sketch open.
        assertEquals("no sketch is open after a load", NativeViewport.SKETCH_INACTIVE,
                sketchState());
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

    private void extrude() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.extrude_sketch).performClick();
            return null;
        });
        settleLayout();
        assertEquals(NativeViewport.SKETCH_INACTIVE, sketchState());
    }

    private double[] toolState() {
        final double[] state = new double[NativeViewport.CAD_EXTRUDE_SIZE];
        NativeViewport.cadExtrudeToolState(state);
        return state;
    }

    private void chooseExtent(final int chipId) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.cadExtrudeCanvas().findViewById(chipId).performClick();
            return null;
        });
        settleLayout();
    }

    /** Opens the PRIMARY value's editor, types, and applies. */
    private void typeFirst(final String meters) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.cadExtrudeCanvas().findViewById(R.id.cad_extrude_depth_value).performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final EditText field = workspace.cadExtrudeCanvas()
                    .findViewById(R.id.field_cad_extrude_depth);
            assertNotNull("the primary field is open", field);
            field.setText(meters);
            workspace.cadExtrudeCanvas().findViewById(R.id.apply_cad_extrude_depth).performClick();
            return null;
        });
        settleLayout();
    }

    /** Opens the SECOND side's editor, types, and applies. */
    private void typeSecond(final String meters) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.cadExtrudeCanvas().findViewById(R.id.cad_extrude_second_value).performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final EditText field = workspace.cadExtrudeCanvas()
                    .findViewById(R.id.field_cad_extrude_second);
            assertNotNull("the second field is open", field);
            field.setText(meters);
            workspace.cadExtrudeCanvas().findViewById(R.id.apply_cad_extrude_second).performClick();
            return null;
        });
        settleLayout();
    }

    private void selectBody(final long id) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.sceneSelectBody(id);
            return null;
        });
        settleLayout();
    }

    /**
     * A one-finger drag from one projected point PAST another, along the line
     * between them — which is the arrow's own shaft, in the arrow's own
     * direction. Both ends come from native's projection.
     */
    private void dragAlong(double fromX, double fromY, double towardX, double towardY) {
        final double dx = towardX - fromX;
        final double dy = towardY - fromY;
        final double length = Math.hypot(dx, dy);
        assertTrue("the shaft has real screen extent", length > 8.0);
        // Half the shaft again past the point aimed at: far enough to be a real
        // change, short enough to stay on screen.
        final float toX = (float) (fromX + dx * 1.5);
        final float toY = (float) (fromY + dy * 1.5);
        dragViewport((float) fromX, (float) fromY, toX, toY);
    }

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
