package com.forgeshape.app;

import static com.forgeshape.app.SketchTestSupport.dragSketch;
import static com.forgeshape.app.SketchTestSupport.selectTool;
import static com.forgeshape.app.SketchTestSupport.sketchEntityCount;
import static com.forgeshape.app.SketchTestSupport.sketchState;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.graphics.Bitmap;
import android.os.SystemClock;
import android.util.Log;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * `CAD-V6-REVOLVE-NEWBODY-E2E-R1` on the device, on the OWNER's own path:
 * Home → New Project → CAD, a square and a separate Line drawn on the canvas,
 * Finish, then Revolve from the precision surface, the axis chosen by a REAL
 * window tap on the drawn edge, the angle typed on the canvas label or dragged
 * on the ring handle, and the commit taken from the toolbar.
 *
 * <p>Asserted from native truth — the revolve tool state, the candidate and
 * body measures, the feature info, the encoded project and the debug tap
 * attribution tokens — and from the status line's own text, never from a
 * picture. The screenshots are evidence only.
 *
 * <p>DEV-REV-09's MULTIFACE half (a fill selection past sixteen areas) is
 * {@code CadMultiFaceOwnerTest}'s, which runs in the same union; this class
 * proves the Extrude path the Revolve sits beside is unchanged.
 */
@RunWith(AndroidJUnit4.class)
public final class CadRevolveOwnerTest {

    private static final String TAG = "ForgeShape";
    /** The square's side, metres; its near edge stands {@link #NEAR} from the axis. */
    private static final double SIDE = 0.8;
    private static final double NEAR = 0.4;
    /** The relative tolerance of a 32-segment revolve against Pappus (sin x / x at 2π/32 is 0.9936). */
    private static final double PAPPUS_TOLERANCE = 0.015;

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private File outDir;
    private final List<String> facts = new ArrayList<>();
    private int marks;

    /** The drawn sketch: the square's id and exact geometry, and the lines. */
    private long square;
    private double squareCu;
    private double squareCv;
    private long axisLine;
    private long crossingLine;

    @Before
    public void startAtHome() {
        outDir = new File(InstrumentationRegistry.getInstrumentation().getTargetContext()
                .getExternalFilesDir(null), "evidence/cad-v6-revolve-newbody");
        //noinspection ResultOfMethodCallIgnored
        outDir.mkdirs();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.dismissRecoveryPromptForTest();
            NativeViewport.supportChooserCancel();
            NativeViewport.sketchCancel();
            NativeViewport.enterConstructionMode();
            workspace.showHomeAsFirstLaunchForTest();
            return null;
        });
        settleLayout();
    }

    @After
    public void restoreAProject() {
        writeFacts();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.supportChooserCancel();
            NativeViewport.sketchCancel();
            NativeViewport.enterConstructionMode();
            return null;
        });
        resetToBaselineConstruction(rule.getScenario());
    }

    // =======================================================================
    // DEV-REV-01: 360 New Project
    // =======================================================================

    @Test
    public void devRev01_a_new_cad_project_revolves_a_square_360_about_a_tapped_line() {
        newSquareAndLineSketch(false);
        assertEquals("the one region is chosen for the user", 1,
                (int) revolveState()[NativeViewport.REVOLVE_SELECTED_AREAS]);
        beginRevolve();
        final double[] picking = revolveState();
        assertEquals("Revolve opens on the axis pick", 1.0,
                picking[NativeViewport.REVOLVE_AXIS_PICKING], 0.0);
        assertEquals("no axis yet", NativeViewport.CAD_REVOLVE_NEEDS_AXIS,
                (int) picking[NativeViewport.REVOLVE_CANDIDATE_STATUS]);
        assertEquals("the status asks for an axis", string(R.string.status_revolve_choose_axis),
                statusLine());
        assertFalse("no commit without an axis", shown(R.id.revolve_sketch));
        capture("03_axis_highlight");

        final List<String> tokens = tapAxis(axisLine, 0.0, -0.6, 0.0, 0.6);
        assertTrue("the axis tap resolved: " + tokens,
                tokens.contains("FORGESHAPE_SKETCH_TAP:resolved"));
        final double[] chosen = revolveState();
        fact("rev01.chosen", Arrays.toString(chosen));
        assertEquals("the tapped Line is the axis", axisLine,
                (long) chosen[NativeViewport.REVOLVE_AXIS_ENTITY]);
        assertEquals("its one edge", 0, (int) chosen[NativeViewport.REVOLVE_AXIS_EDGE]);
        assertEquals("the default is a full turn", 360.0, chosen[NativeViewport.REVOLVE_ANGLE], 0.0);
        assertEquals(1.0, chosen[NativeViewport.REVOLVE_FULL_TURN], 0.0);
        assertEquals(NativeViewport.CAD_OK, (int) chosen[NativeViewport.REVOLVE_CANDIDATE_STATUS]);
        assertTrue("the angle label stands on the ring", labelShown());
        assertEquals("360°", labelText());

        // The preview IS the candidate: a tube of the square about u = 0.
        final double[] candidate = candidateMeasure();
        final double pappus = 2.0 * Math.PI * (NEAR + 0.5 * SIDE) * SIDE * SIDE;
        fact("rev01.candidate", Arrays.toString(candidate) + " pappus=" + pappus);
        assertEquals("one closed solid", 1.0, candidate[NativeViewport.CANDIDATE_MEASURE_COMPONENTS],
                0.0);
        assertEquals(pappus, candidate[NativeViewport.CANDIDATE_MEASURE_VOLUME],
                pappus * PAPPUS_TOLERANCE);
        assertTrue("Revolve is the commit", shown(R.id.revolve_sketch));
        assertFalse("Extrude is not drawn beside it", shown(R.id.extrude_sketch));
        capture("01_360_preview");
        capture("04_angle_label");

        commitFromToolbar();
        assertTrue("the first Revolve creates the project", NativeViewport.projectOpen());
        assertEquals(NativeViewport.SKETCH_INACTIVE, sketchState());
        final long body = NativeViewport.sceneActiveBodyId();
        final double[] cad = cadState();
        assertEquals(NativeViewport.FEATURE_KIND_REVOLVE, (int) cad[NativeViewport.CAD_STATE_KIND]);
        assertEquals(360.0, cad[NativeViewport.CAD_STATE_REVOLVE_ANGLE], 0.0);
        final double[] measured = measure(body);
        fact("rev01.body", Arrays.toString(measured));
        assertEquals("the committed body is the previewed one",
                candidate[NativeViewport.CANDIDATE_MEASURE_VOLUME],
                measured[NativeViewport.CAD_MEASURE_VOLUME], 1e-9);
        assertEquals(1.0, measured[NativeViewport.CAD_MEASURE_COMPONENTS], 0.0);
        fact("rev01.status", statusLine());
        capture("05_committed_body");
    }

    // =======================================================================
    // DEV-REV-02: an exact partial angle, then an exact edit
    // =======================================================================

    @Test
    public void devRev02_an_exact_partial_angle_commits_and_reopens_for_an_exact_edit() {
        final long body = revolvedProject(90.0, false);
        assertEquals(90.0, featureInfo(body)[NativeViewport.CAD_FEATURE_ANGLE], 0.0);
        final int bodies = NativeViewport.sceneBodyCount();

        reopenRevolveFeature();
        final double[] reopened = revolveState();
        assertEquals("it reopens as the Revolve it is", 1.0,
                reopened[NativeViewport.REVOLVE_EDITING_REVOLVE_BODY], 0.0);
        assertEquals(90.0, reopened[NativeViewport.REVOLVE_ANGLE], 0.0);
        assertEquals(axisLine, (long) reopened[NativeViewport.REVOLVE_AXIS_ENTITY]);
        assertFalse("no Extrude instead over a revolved body",
                shownInSketchEditor(R.id.sketch_revolve_back_to_extrude));
        capture("06_reopened_revolve");

        typeAngleOnCanvas("180");
        assertEquals(180.0, revolveState()[NativeViewport.REVOLVE_ANGLE], 0.0);
        assertEquals("180°", labelText());
        final int undo = NativeViewport.constructionUndoDepth();
        commitFromToolbar();
        assertEquals(NativeViewport.SKETCH_INACTIVE, sketchState());
        assertEquals("the same body, edited", body, NativeViewport.sceneActiveBodyId());
        assertEquals("no second body", bodies, NativeViewport.sceneBodyCount());
        assertEquals("one Finish is one step", undo + 1, NativeViewport.constructionUndoDepth());
        final double[] info = featureInfo(body);
        assertEquals(NativeViewport.FEATURE_KIND_REVOLVE, (int) info[NativeViewport.CAD_FEATURE_KIND]);
        assertEquals("exactly the typed angle", 180.0, info[NativeViewport.CAD_FEATURE_ANGLE], 0.0);
    }

    // =======================================================================
    // DEV-REV-03: a real ring-handle drag changes the angle and orbits nothing
    // =======================================================================

    @Test
    public void devRev03_a_real_ring_handle_drag_changes_the_angle_without_orbiting() {
        newSquareAndLineSketch(false);
        beginRevolve();
        tapAxis(axisLine, 0.0, -0.6, 0.0, 0.6);
        typeAngleOnCanvas("90");
        capture("02_90_preview");
        final double[] before = revolveState();
        assertEquals(1.0, before[NativeViewport.REVOLVE_HANDLE_VISIBLE], 0.0);
        assertEquals(1.0, before[NativeViewport.REVOLVE_LABEL_VISIBLE], 0.0);
        final float hx = (float) before[NativeViewport.REVOLVE_HANDLE_X];
        final float hy = (float) before[NativeViewport.REVOLVE_HANDLE_Y];
        // The label stands at HALF the angle, a little outside the ring: a
        // straight drag from the handle to it walks the ring back towards 45°.
        final float lx = (float) before[NativeViewport.REVOLVE_LABEL_X];
        final float ly = (float) before[NativeViewport.REVOLVE_LABEL_Y];
        assertNotNull("the handle is reachable: " + lastBlock, viewportPoint(new float[]{hx, hy}));
        final float[] poseBefore = cameraPose();
        final int steps = 12;
        final float[][] path = new float[steps + 1][];
        for (int i = 0; i <= steps; i++) {
            final float t = (float) i / steps;
            path[i] = new float[]{hx + (lx - hx) * t, hy + (ly - hy) * t};
        }
        realGesture(path);
        final double[] after = revolveState();
        fact("rev03.drag", "from " + before[NativeViewport.REVOLVE_ANGLE] + " to "
                + after[NativeViewport.REVOLVE_ANGLE] + " pose " + Arrays.toString(poseBefore)
                + " -> " + Arrays.toString(cameraPose()));
        assertArrayEquals("a handle drag orbits nothing", poseBefore, cameraPose(), 0.0f);
        final double angle = after[NativeViewport.REVOLVE_ANGLE];
        assertTrue("the angle followed the finger back towards 45°: " + angle,
                angle >= 35.0 && angle <= 55.0);
        assertEquals("a drag lands on whole degrees", Math.rint(angle), angle, 0.0);
        assertEquals("the drag ended", 0.0, after[NativeViewport.REVOLVE_DRAGGING], 0.0);
        assertEquals(NativeViewport.CAD_OK, (int) after[NativeViewport.REVOLVE_CANDIDATE_STATUS]);
        assertEquals(CadRevolvePresentation.label(angle), labelText());

        // A still tap on the profile beside the ring is not the handle's: it
        // toggles the area off and back on, the angle untouched.
        final float[] inside = sketchPointOnViewport(squareCu, squareCv);
        assertNotNull("the square is reachable: " + lastBlock, inside);
        realGesture(new float[][]{inside});
        assertEquals("the tap reached the area", 0,
                (int) revolveState()[NativeViewport.REVOLVE_SELECTED_AREAS]);
        realGesture(new float[][]{inside});
        assertEquals(1, (int) revolveState()[NativeViewport.REVOLVE_SELECTED_AREAS]);
        assertEquals(angle, revolveState()[NativeViewport.REVOLVE_ANGLE], 0.0);
    }

    // =======================================================================
    // DEV-REV-04: Flip keeps the magnitude and reverses the sweep
    // =======================================================================

    @Test
    public void devRev04_flip_keeps_the_angle_and_mirrors_the_sweep_deterministically() {
        newSquareAndLineSketch(false);
        beginRevolve();
        tapAxis(axisLine, 0.0, -0.6, 0.0, 0.6);
        typeAngleOnCanvas("90");
        final double[] positive = candidateMeasureRaw();
        assertEquals(NativeViewport.REVOLVE_POSITIVE,
                (int) revolveState()[NativeViewport.REVOLVE_DIRECTION]);
        pressInSketchEditor(R.id.sketch_revolve_flip);
        final double[] flipped = revolveState();
        assertEquals("Flip reverses the direction", NativeViewport.REVOLVE_NEGATIVE,
                (int) flipped[NativeViewport.REVOLVE_DIRECTION]);
        assertEquals("and keeps the exact angle", 90.0, flipped[NativeViewport.REVOLVE_ANGLE], 0.0);
        final double[] negative = candidateMeasureRaw();
        fact("rev04", Arrays.toString(positive) + " / " + Arrays.toString(negative));
        assertEquals("the same volume", positive[NativeViewport.CANDIDATE_MEASURE_VOLUME],
                negative[NativeViewport.CANDIDATE_MEASURE_VOLUME], 1e-9);
        // The quarter sweep leaves the XY plane on the other side: z mirrors.
        final int minZ = NativeViewport.CANDIDATE_MEASURE_MIN_X + 2;
        final int maxZ = NativeViewport.CANDIDATE_MEASURE_MAX_X + 2;
        assertEquals("z mirrors", -positive[maxZ], negative[minZ], 1e-6);
        assertEquals("z mirrors", -positive[minZ], negative[maxZ], 1e-6);
        assertNotEquals("the sweep moved", positive[maxZ], negative[maxZ], 1e-6);
        pressInSketchEditor(R.id.sketch_revolve_flip);
        assertArrayEquals("Flip twice is where it started",
                Arrays.copyOfRange(positive, 1, 10), Arrays.copyOfRange(candidateMeasureRaw(), 1, 10),
                0.0);
    }

    // =======================================================================
    // DEV-REV-05: changing the axis to another straight edge regenerates
    // =======================================================================

    @Test
    public void devRev05_a_second_straight_edge_as_the_axis_regenerates_the_preview() {
        newSquareAndLineSketch(false);
        beginRevolve();
        tapAxis(axisLine, 0.0, -0.6, 0.0, 0.6);
        final double tube = candidateMeasure()[NativeViewport.CANDIDATE_MEASURE_VOLUME];
        pressInSketchEditor(R.id.sketch_revolve_change_axis);
        assertEquals(1.0, revolveState()[NativeViewport.REVOLVE_AXIS_PICKING], 0.0);
        // The square's own near edge, u = NEAR: the profile TOUCHES its axis,
        // which is allowed -- a solid cylinder of radius SIDE.
        final double edgeU = squareCu - 0.5 * SIDE;
        tapAxis(square, edgeU, squareCv - 0.3 * SIDE, edgeU, squareCv + 0.3 * SIDE);
        final double[] state = revolveState();
        assertEquals("the square's edge is the axis now", square,
                (long) state[NativeViewport.REVOLVE_AXIS_ENTITY]);
        assertEquals(NativeViewport.CAD_OK, (int) state[NativeViewport.REVOLVE_CANDIDATE_STATUS]);
        final double cylinder = candidateMeasure()[NativeViewport.CANDIDATE_MEASURE_VOLUME];
        final double pappus = Math.PI * SIDE * SIDE * SIDE;
        fact("rev05", "tube=" + tube + " cylinder=" + cylinder + " pappus=" + pappus
                + " edge=" + (int) state[NativeViewport.REVOLVE_AXIS_EDGE]);
        assertEquals(pappus, cylinder, pappus * PAPPUS_TOLERANCE);
        assertNotEquals("the preview regenerated", tube, cylinder, 1e-6);
    }

    // =======================================================================
    // DEV-REV-06: an axis whose line crosses the area is refused by name
    // =======================================================================

    @Test
    public void devRev06_a_crossing_axis_is_refused_by_name_and_commits_nothing() {
        newSquareAndLineSketch(true);
        beginRevolve();
        // The short Line above the square: its LINE passes through the square.
        tapAxis(crossingLine, squareCu, squareCv + 0.5 * SIDE + 0.15, squareCu,
                squareCv + 0.5 * SIDE + 0.45);
        final double[] state = revolveState();
        assertEquals(crossingLine, (long) state[NativeViewport.REVOLVE_AXIS_ENTITY]);
        assertEquals("refused by name", NativeViewport.CAD_REVOLVE_PROFILE_CROSSES_AXIS,
                (int) state[NativeViewport.REVOLVE_CANDIDATE_STATUS]);
        final String refusal = onWorkspace(rule.getScenario(), (activity, workspace) ->
                CadStatusMessages.describe(activity, NativeViewport.CAD_REVOLVE_PROFILE_CROSSES_AXIS));
        assertEquals("the status names the refusal", refusal, statusLine());
        assertFalse("a refused candidate has no commit", shown(R.id.revolve_sketch));
        // The precision surface's pinned commit submits and is refused below JNI.
        final boolean pinned = shownInSketchEditor(R.id.sketch_revolve_commit);
        fact("rev06.pinned_commit_shown", pinned);
        if (pinned) {
            pressInSketchEditor(R.id.sketch_revolve_commit);
            assertEquals(NativeViewport.CAD_REVOLVE_PROFILE_CROSSES_AXIS,
                    NativeViewport.sketchLastStatus());
        }
        assertFalse("nothing was created", NativeViewport.projectOpen());
        assertEquals(NativeViewport.SKETCH_READY, sketchState());

        // A valid axis commits.
        pressInSketchEditor(R.id.sketch_revolve_change_axis);
        tapAxis(axisLine, 0.0, -0.6, 0.0, 0.6);
        assertEquals(NativeViewport.CAD_OK,
                (int) revolveState()[NativeViewport.REVOLVE_CANDIDATE_STATUS]);
        commitFromToolbar();
        assertTrue(NativeViewport.projectOpen());
        assertEquals(NativeViewport.FEATURE_KIND_REVOLVE,
                (int) cadState()[NativeViewport.CAD_STATE_KIND]);
    }

    // =======================================================================
    // DEV-REV-07: Undo / Redo restore the exact feature
    // =======================================================================

    @Test
    public void devRev07_undo_and_redo_restore_the_exact_revolve() {
        final long body = revolvedProject(360.0, false);
        final double full = measure(body)[NativeViewport.CAD_MEASURE_VOLUME];
        reopenRevolveFeature();
        typeAngleOnCanvas("37.5");
        pressInSketchEditor(R.id.sketch_revolve_flip);
        commitFromToolbar();
        final double[] edited = featureInfo(body);
        assertEquals(37.5, edited[NativeViewport.CAD_FEATURE_ANGLE], 0.0);
        assertEquals(NativeViewport.REVOLVE_NEGATIVE,
                (int) edited[NativeViewport.CAD_FEATURE_REVOLVE_DIRECTION]);
        final double partial = measure(body)[NativeViewport.CAD_MEASURE_VOLUME];
        assertTrue("37.5° is a fraction of the tube", partial < full * 0.2);

        press(R.id.undo_action);
        final double[] undone = featureInfo(body);
        assertEquals("Undo restores 360 exactly", 360.0, undone[NativeViewport.CAD_FEATURE_ANGLE], 0.0);
        assertEquals(NativeViewport.REVOLVE_POSITIVE,
                (int) undone[NativeViewport.CAD_FEATURE_REVOLVE_DIRECTION]);
        assertEquals(full, measure(body)[NativeViewport.CAD_MEASURE_VOLUME], 0.0);

        press(R.id.redo_action);
        final double[] redone = featureInfo(body);
        assertEquals("Redo restores 37.5 exactly", 37.5, redone[NativeViewport.CAD_FEATURE_ANGLE], 0.0);
        assertEquals(NativeViewport.REVOLVE_NEGATIVE,
                (int) redone[NativeViewport.CAD_FEATURE_REVOLVE_DIRECTION]);
        assertEquals(partial, measure(body)[NativeViewport.CAD_MEASURE_VOLUME], 0.0);
    }

    // =======================================================================
    // DEV-REV-08: save and reopen keep the body, the angle, the direction,
    // the axis ref and the feature
    // =======================================================================

    @Test
    public void devRev08_save_and_reopen_keep_the_revolve_whole() {
        final long body = revolvedProject(90.0, true);
        final double volume = measure(body)[NativeViewport.CAD_MEASURE_VOLUME];
        final byte[] saved = NativeViewport.encodeProject();
        assertNotNull(saved);
        assertEquals("a revolved project is written as CADB v7", 7, cadbVersion(saved));
        final int loaded = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int status = NativeViewport.loadProject(saved);
            workspace.onNativeStateChanged();
            return status;
        });
        settleLayout();
        fact("rev08.load", loaded + " bytes=" + saved.length);
        assertTrue("it reopens", NativeViewport.projectOpen());
        final long reopened = NativeViewport.sceneActiveBodyId();
        assertEquals("the same body", body, reopened);
        assertEquals(1, NativeViewport.cadFeatureCount(reopened));
        final double[] info = featureInfo(reopened);
        assertEquals(NativeViewport.FEATURE_KIND_REVOLVE, (int) info[NativeViewport.CAD_FEATURE_KIND]);
        assertEquals(90.0, info[NativeViewport.CAD_FEATURE_ANGLE], 0.0);
        assertEquals(NativeViewport.REVOLVE_NEGATIVE,
                (int) info[NativeViewport.CAD_FEATURE_REVOLVE_DIRECTION]);
        assertEquals("the same solid", volume, measure(reopened)[NativeViewport.CAD_MEASURE_VOLUME],
                0.0);
        assertEquals("a fresh history, as every Open", 0, NativeViewport.constructionUndoDepth());
        // The axis ref, read where the user meets it: the reopened feature.
        reopenRevolveFeature();
        final double[] state = revolveState();
        assertEquals("the axis is the same Line", axisLine,
                (long) state[NativeViewport.REVOLVE_AXIS_ENTITY]);
        assertEquals(0, (int) state[NativeViewport.REVOLVE_AXIS_EDGE]);
        assertEquals(90.0, state[NativeViewport.REVOLVE_ANGLE], 0.0);
        assertEquals(NativeViewport.REVOLVE_NEGATIVE, (int) state[NativeViewport.REVOLVE_DIRECTION]);
        press(R.id.cancel_sketch);
        assertEquals(NativeViewport.SKETCH_INACTIVE, sketchState());
        assertArrayEquals("Cancel cost the project nothing", saved, NativeViewport.encodeProject());
    }

    // =======================================================================
    // DEV-REV-09: the Extrude the Revolve sits beside still commits
    // =======================================================================

    @Test
    public void devRev09_the_extrude_beside_revolve_still_commits_with_its_hud() {
        newSquareAndLineSketch(false);
        assertTrue("Extrude is the default transition", shown(R.id.extrude_sketch));
        assertTrue("its value stands on the canvas", onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.cadExtrudeCanvas()
                        .findViewById(R.id.cad_extrude_depth_value).isShown()));
        // Into Revolve and back out: the extrusion is what it was.
        beginRevolve();
        pressInSketchEditor(R.id.sketch_revolve_back_to_extrude);
        assertEquals(0.0, revolveState()[NativeViewport.REVOLVE_ACTIVE], 0.0);
        assertTrue(shown(R.id.extrude_sketch));
        press(R.id.extrude_sketch);
        assertTrue(NativeViewport.projectOpen());
        assertEquals(NativeViewport.FEATURE_KIND_EXTRUDE,
                (int) cadState()[NativeViewport.CAD_STATE_KIND]);
        final byte[] bytes = NativeViewport.encodeProject();
        assertTrue("an extrude-only project never writes v7", cadbVersion(bytes) < 7);
    }

    // -----------------------------------------------------------------------
    // The sketch: New Project -> CAD, a square right of a vertical Line
    // -----------------------------------------------------------------------

    /**
     * Draws the square [NEAR, NEAR + SIDE] x [-SIDE/2, SIDE/2] and the Line
     * u = 0, v in [-0.6, 0.6] -- and, when asked, a short second Line above
     * the square on its centre line, whose LINE crosses it -- then Finish.
     * Each entity is placed by a real drag and then TYPED exact through the
     * path the Sketch values surface uses, as the MULTIFACE driver does.
     */
    private void newSquareAndLineSketch(boolean withCrossingLine) {
        press(R.id.home_new_project);
        press(R.id.new_project_cad);
        assertEquals("New CAD lands in a sketch", NativeViewport.SKETCH_EDITING, sketchState());
        assertFalse(NativeViewport.projectOpen());
        settleLayout();
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), NEAR, -0.5 * SIDE, NEAR + SIDE, 0.5 * SIDE);
        final double[] drawn = new double[NativeViewport.SKETCH_ENTITY_SIZE];
        assertTrue("the new rectangle is selected", NativeViewport.sketchSelectedEntity(drawn));
        assertEquals(NativeViewport.SKETCH_ENTITY_KIND_RECTANGLE,
                (int) drawn[NativeViewport.SKETCH_ENTITY_KIND]);
        square = (long) drawn[NativeViewport.SKETCH_ENTITY_ID];
        assertEquals(NativeViewport.CAD_OK,
                applyOnUi(() -> NativeViewport.sketchApplyRectangle(square, SIDE, SIDE)));
        assertTrue(NativeViewport.sketchSelectedEntity(drawn));
        squareCu = drawn[NativeViewport.SKETCH_ENTITY_VALUES];
        squareCv = drawn[NativeViewport.SKETCH_ENTITY_VALUES + 1];
        fact("square", "id=" + square + " centre=(" + squareCu + ", " + squareCv + ")");
        // The drag snaps to the view's grid: the square must stand clear of the
        // axis for the tube this class measures.
        assertTrue("the square stands right of u = 0", squareCu - 0.5 * SIDE > 0.05);
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        axisLine = placeLine(0.0, -0.6, 0.0, 0.6);
        if (withCrossingLine) {
            final double top = squareCv + 0.5 * SIDE;
            crossingLine = placeLine(squareCu, top + 0.15, squareCu, top + 0.45);
        }
        assertEquals(withCrossingLine ? 3 : 2, sketchEntityCount());
        press(R.id.finish_sketch);
        assertEquals("Finish succeeds: " + NativeViewport.sketchLastStatus(),
                NativeViewport.SKETCH_READY, sketchState());
    }

    /** A line by a real drag, then typed to its exact endpoints. */
    private long placeLine(double u0, double v0, double u1, double v1) {
        final int before = sketchEntityCount();
        dragSketch(rule.getScenario(), u0, v0, u1, v1);
        assertEquals("one line placed", before + 1, sketchEntityCount());
        final double[] drawn = new double[NativeViewport.SKETCH_ENTITY_SIZE];
        assertTrue("the new line is selected", NativeViewport.sketchSelectedEntity(drawn));
        assertEquals(NativeViewport.SKETCH_ENTITY_KIND_LINE,
                (int) drawn[NativeViewport.SKETCH_ENTITY_KIND]);
        final long id = (long) drawn[NativeViewport.SKETCH_ENTITY_ID];
        assertEquals(NativeViewport.CAD_OK,
                applyOnUi(() -> NativeViewport.sketchApplyLine(id, u0, v0, u1, v1)));
        return id;
    }

    /** A committed revolve project: the square about the Line at an exact angle. */
    private long revolvedProject(double degrees, boolean flip) {
        newSquareAndLineSketch(false);
        beginRevolve();
        tapAxis(axisLine, 0.0, -0.6, 0.0, 0.6);
        if (degrees != 360.0) {
            typeAngleOnCanvas(CadRevolvePresentation.formatDegrees(degrees));
        }
        if (flip) {
            pressInSketchEditor(R.id.sketch_revolve_flip);
        }
        commitFromToolbar();
        assertTrue("the revolve created the project: " + statusLine(), NativeViewport.projectOpen());
        final long body = NativeViewport.sceneActiveBodyId();
        assertEquals(NativeViewport.FEATURE_KIND_REVOLVE,
                (int) featureInfo(body)[NativeViewport.CAD_FEATURE_KIND]);
        return body;
    }

    /** Revolve… from the precision surface; the surface is closed again after. */
    private void beginRevolve() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openPrecision(workspace);
            return null;
        });
        settleLayout();
        assertTrue("Revolve… is offered over a chosen area",
                shownInSketchEditor(R.id.sketch_revolve_begin));
        pressInSketchEditor(R.id.sketch_revolve_begin);
        assertEquals(1.0, revolveState()[NativeViewport.REVOLVE_ACTIVE], 0.0);
        closePrecision();
    }

    /** The revolved body's feature row in the precision surface: reopens it staged. */
    private void reopenRevolveFeature() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onNativeStateChanged();
            WorkspaceTestSupport.openPrecision(workspace);
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final ViewGroup list = workspace.findViewById(R.id.cad_feature_list);
            assertNotNull("the feature list exists", list);
            assertTrue("the feature list is shown", list.isShown());
            final List<View> rows = new ArrayList<>();
            collectById(list, R.id.cad_feature_row, rows);
            assertEquals("one feature row: the Revolve", 1, rows.size());
            final String text = ((TextView) rows.get(0)).getText().toString();
            fact("feature_row", text);
            rows.get(0).performClick();
            return null;
        });
        settleLayout();
        assertEquals("it opens in Ready", NativeViewport.SKETCH_READY, sketchState());
        assertEquals(1.0, revolveState()[NativeViewport.REVOLVE_ACTIVE], 0.0);
        closePrecision();
    }

    private static void collectById(View view, int id, List<View> out) {
        if (view.getId() == id) {
            out.add(view);
        }
        if (view instanceof ViewGroup) {
            final ViewGroup group = (ViewGroup) view;
            for (int i = 0; i < group.getChildCount(); i++) {
                collectById(group.getChildAt(i), id, out);
            }
        }
    }

    private void closePrecision() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.closePrecision(workspace);
            return null;
        });
        settleLayout();
    }

    /**
     * One still real tap on the edge [p0, p1] of entity {@code id}: the first
     * of five points along it that is on the viewport and under no chrome.
     * Asserts the axis became that entity; returns the tap tokens.
     */
    private List<String> tapAxis(long id, double u0, double v0, double u1, double v1) {
        float[] at = null;
        for (double t : new double[]{0.5, 0.35, 0.65, 0.2, 0.8}) {
            at = sketchPointOnViewport(u0 + (u1 - u0) * t, v0 + (v1 - v0) * t);
            if (at != null) break;
        }
        assertNotNull("the axis edge is reachable: " + lastBlock, at);
        final String mark = mark();
        realGesture(new float[][]{at});
        final List<String> tokens = tokensSince(mark);
        final double[] state = revolveState();
        fact("axis_tap." + id, Arrays.toString(at) + " tokens=" + tokens + " state="
                + Arrays.toString(state));
        assertEquals("the tap chose entity " + id + " (last " + NativeViewport.sketchLastStatus()
                + ", tokens " + tokens + ")", id, (long) state[NativeViewport.REVOLVE_AXIS_ENTITY]);
        assertEquals("the pick is over", 0.0, state[NativeViewport.REVOLVE_AXIS_PICKING], 0.0);
        return tokens;
    }

    /** The canvas label: open its editor, type, Apply -- the user's exact path. */
    private void typeAngleOnCanvas(String text) {
        assertTrue("the angle label stands", labelShown());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.cadRevolveAngleLabel().findViewById(R.id.cad_revolve_angle_value).performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final CadRevolveAngleLabelView label = workspace.cadRevolveAngleLabel();
            assertTrue("the editor opened", label.editorOpen());
            final EditText field = label.findViewById(R.id.field_revolve_angle_canvas);
            field.setText(text);
            label.findViewById(R.id.apply_revolve_angle).performClick();
            return null;
        });
        settleLayout();
        assertEquals("the typed angle was accepted: " + statusLine(), NativeViewport.CAD_OK,
                NativeViewport.sketchLastStatus());
    }

    private void commitFromToolbar() {
        assertTrue("the toolbar's Revolve is drawn: " + statusLine(), shown(R.id.revolve_sketch));
        press(R.id.revolve_sketch);
    }

    // -----------------------------------------------------------------------
    // Taps and drags through the WINDOW
    // -----------------------------------------------------------------------

    private String lastBlock = "";

    private float[] sketchPointOnViewport(double u, double v) {
        final float[] at = new float[2];
        if (!NativeViewport.sketchScreenPoint(u, v, at)) {
            lastBlock += "[(" + u + "," + v + ") does not project]";
            return null;
        }
        return viewportPoint(at);
    }

    /** The point when it is on the viewport and no clickable chrome stands over it. */
    private float[] viewportPoint(float[] at) {
        final String block = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final float density = activity.getResources().getDisplayMetrics().density;
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            if (at[0] < 8 * density || at[1] < 8 * density
                    || at[0] > viewport.getWidth() - 8 * density
                    || at[1] > viewport.getHeight() - 8 * density) {
                return "off the viewport";
            }
            final View root = activity.getWindow().getDecorView();
            final int[] vp = new int[2];
            viewport.getLocationInWindow(vp);
            final View owner = clickableAt(root, at[0] + vp[0], at[1] + vp[1]);
            if (owner == null || owner == viewport) {
                return null;
            }
            return "under " + owner.getClass().getSimpleName();
        });
        lastBlock = block == null ? "" : lastBlock + "[" + at[0] + "," + at[1] + " " + block + "]";
        return block == null ? at : null;
    }

    private static View clickableAt(View view, float wx, float wy) {
        if (view.getVisibility() != View.VISIBLE) {
            return null;
        }
        final int[] at = new int[2];
        view.getLocationInWindow(at);
        if (wx < at[0] || wy < at[1] || wx >= at[0] + view.getWidth()
                || wy >= at[1] + view.getHeight()) {
            return null;
        }
        if (view instanceof ViewGroup) {
            final ViewGroup group = (ViewGroup) view;
            for (int i = group.getChildCount() - 1; i >= 0; i--) {
                final View hit = clickableAt(group.getChildAt(i), wx, wy);
                if (hit != null) {
                    return hit;
                }
            }
        }
        return view.isClickable() || view.getId() == R.id.viewport_surface ? view : null;
    }

    /** One finger through the window: Down, a Move per later point, Up at the last. */
    private void realGesture(final float[][] path) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final View root = activity.getWindow().getDecorView();
            final int[] vp = new int[2];
            final int[] rp = new int[2];
            viewport.getLocationInWindow(vp);
            root.getLocationInWindow(rp);
            final float ox = vp[0] - rp[0];
            final float oy = vp[1] - rp[1];
            final long down = SystemClock.uptimeMillis();
            dispatch(root, down, down, MotionEvent.ACTION_DOWN, path[0][0] + ox, path[0][1] + oy);
            for (int i = 1; i < path.length; i++) {
                dispatch(root, down, down + 16L * i, MotionEvent.ACTION_MOVE, path[i][0] + ox,
                        path[i][1] + oy);
            }
            final float[] last = path[path.length - 1];
            dispatch(root, down, down + 16L * path.length + 24L, MotionEvent.ACTION_UP,
                    last[0] + ox, last[1] + oy);
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

    // -----------------------------------------------------------------------
    // Plumbing
    // -----------------------------------------------------------------------

    private void press(final int id) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View control = workspace.findViewById(id);
            assertNotNull("control " + id + " must exist", control);
            assertTrue("control " + id + " must be on screen", control.isShown());
            control.performClick();
            return null;
        });
        settleLayout();
    }

    /** A control of the sketch's precision surface, opened first when it is closed. */
    private void pressInSketchEditor(final int id) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openPrecision(workspace);
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View control = workspace.sketchEditor().findViewById(id);
            assertNotNull("control " + id + " must exist", control);
            assertTrue("control " + id + " must be on screen", control.isShown());
            control.performClick();
            return null;
        });
        settleLayout();
        closePrecision();
    }

    private boolean shownInSketchEditor(final int id) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openPrecision(workspace);
            return null;
        });
        settleLayout();
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View control = workspace.sketchEditor().findViewById(id);
            return control != null && control.isShown();
        });
    }

    private boolean shown(final int id) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View control = workspace.findViewById(id);
            return control != null && control.isShown();
        });
    }

    private boolean labelShown() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.cadRevolveAngleLabel().isShown());
    }

    private String labelText() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> ((TextView) workspace
                .cadRevolveAngleLabel().findViewById(R.id.cad_revolve_angle_value)).getText()
                .toString());
    }

    private int applyOnUi(java.util.function.IntSupplier apply) {
        final int status = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int why = apply.getAsInt();
            workspace.onNativeStateChanged();
            return why;
        });
        settleLayout();
        return status;
    }

    private String string(int id) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> activity.getString(id));
    }

    private String statusLine() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View message = workspace.findViewById(R.id.status_message);
            return message instanceof TextView && message.isShown()
                    ? ((TextView) message).getText().toString() : "";
        });
    }

    private static float[] cameraPose() {
        final float[] pose = new float[NativeViewport.CAMERA_POSE_SIZE];
        NativeViewport.debugCameraPose(pose);
        return pose;
    }

    private static double[] revolveState() {
        final double[] state = new double[NativeViewport.REVOLVE_STATE_SIZE];
        NativeViewport.cadRevolveToolState(state);
        return state;
    }

    private static double[] candidateMeasureRaw() {
        final double[] out = new double[NativeViewport.CANDIDATE_MEASURE_SIZE];
        assertTrue("the candidate measures", NativeViewport.sketchCandidateMeasure(out));
        assertEquals("the candidate is valid", NativeViewport.CAD_OK,
                (int) out[NativeViewport.CANDIDATE_MEASURE_STATUS]);
        return out;
    }

    private static double[] candidateMeasure() {
        return candidateMeasureRaw();
    }

    private static double[] measure(long body) {
        final double[] out = new double[NativeViewport.CAD_MEASURE_SIZE];
        assertTrue("the body measures as a CAD solid", NativeViewport.cadBodyMeasure(body, out));
        return out;
    }

    private static double[] cadState() {
        final double[] out = new double[NativeViewport.CAD_STATE_SIZE];
        assertTrue("the active body is a CAD Body", NativeViewport.cadState(out));
        return out;
    }

    private static double[] featureInfo(long body) {
        final double[] out = new double[NativeViewport.CAD_FEATURE_INFO_SIZE];
        assertTrue("the body has its first feature", NativeViewport.cadFeatureInfo(body, 0, out));
        return out;
    }

    /** The `CADB` section's version in an encoded project (DATA_PACKAGE_SPEC.md §3). */
    private static int cadbVersion(byte[] bytes) {
        for (int i = 28; i + 6 <= bytes.length; i++) {
            if (bytes[i] == 'C' && bytes[i + 1] == 'A' && bytes[i + 2] == 'D' && bytes[i + 3] == 'B') {
                return (bytes[i + 4] & 0xFF) | ((bytes[i + 5] & 0xFF) << 8);
            }
        }
        return -1;
    }

    // -----------------------------------------------------------------------
    // Debug attribution tokens, read back from this process's own log
    // -----------------------------------------------------------------------

    private String mark() {
        final String marker = "CADREV_MARK_" + System.nanoTime() + "_" + (marks++);
        Log.i(TAG, marker);
        return marker;
    }

    private static List<String> tokensSince(String marker) {
        final List<String> tokens = new ArrayList<>();
        try {
            final Process process = Runtime.getRuntime().exec(
                    new String[]{"logcat", "-d", "-v", "raw", "-s", "ForgeShape:I"});
            boolean after = false;
            try (BufferedReader in = new BufferedReader(
                    new InputStreamReader(process.getInputStream(), "UTF-8"))) {
                String line;
                while ((line = in.readLine()) != null) {
                    if (line.contains(marker)) {
                        after = true;
                        tokens.clear();
                        continue;
                    }
                    if (!after) continue;
                    final String trimmed = line.trim();
                    if (trimmed.startsWith("FORGESHAPE_SKETCH_TAP:")
                            || trimmed.startsWith("FORGESHAPE_REVOLVE_AXIS_TAP:")) {
                        final int space = trimmed.indexOf(' ');
                        tokens.add(space > 0 ? trimmed.substring(0, space) : trimmed);
                    }
                }
            }
            process.waitFor();
        } catch (IOException | InterruptedException error) {
            tokens.add("logcat_unreadable:" + error);
        }
        return tokens;
    }

    // -----------------------------------------------------------------------
    // Evidence
    // -----------------------------------------------------------------------

    /** Saves the screen after the renderer presented a few new frames; evidence only. */
    private void capture(String name) {
        final long start = NativeViewport.debugRendererFramesPresented();
        final long began = SystemClock.uptimeMillis();
        long seen = 0;
        while (SystemClock.uptimeMillis() - began < 15000L && seen < 6) {
            final long now = NativeViewport.debugRendererFramesPresented();
            seen = now >= start ? now - start : now;
            SystemClock.sleep(50);
        }
        final Bitmap frame = InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .takeScreenshot();
        if (frame == null) {
            fact("capture." + name, "unavailable");
            return;
        }
        final File png = new File(outDir, name + ".png");
        try (FileOutputStream out = new FileOutputStream(png)) {
            frame.compress(Bitmap.CompressFormat.PNG, 100, out);
            fact("capture." + name, png.getName() + " frames=" + seen);
        } catch (IOException error) {
            fact("capture." + name, "write_failed");
        }
    }

    private void fact(String key, Object value) {
        final String line = key + "=" + value;
        facts.add(line);
        Log.i(TAG, "CADREV_DEVICE " + line);
    }

    private void writeFacts() {
        final File file = new File(outDir, "facts-" + System.nanoTime() + ".txt");
        try (PrintWriter out = new PrintWriter(file, "UTF-8")) {
            for (String line : facts) {
                out.println(line);
            }
        } catch (IOException error) {
            Log.w(TAG, "CADREV_DEVICE facts not written: " + error);
        }
    }
}
