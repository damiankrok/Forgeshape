package com.forgeshape.app;

import static com.forgeshape.app.SketchTestSupport.dragSketch;
import static com.forgeshape.app.SketchTestSupport.selectTool;
import static com.forgeshape.app.SketchTestSupport.sketchState;
import static com.forgeshape.app.SketchTestSupport.tapViewport;
import static com.forgeshape.app.SketchTestSupport.tapWorld;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.util.Log;
import android.view.View;
import android.widget.EditText;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Arrays;

/**
 * `CAD-V6-S2` on the device: a sketch whose curves CROSS extrudes the atomic
 * planar faces the user taps, through the same controls every extrusion uses.
 *
 * <p>Six journeys: a crossing circle's lens (J1), a protrusion made of every
 * face (J2), two overlapping circles (J3), a union across a shared fragment
 * (J4), save/reopen and feature reopen (J5), and a face selection feeding
 * same-body Add and Cut (J6). Each is asserted from native truth — the
 * selection kind the feature stores and the regenerated solid's volume and
 * shells — never from a picture. Faces are named here by AREA, because their
 * row handles are transient and a sketch coordinate is not how a user picks.
 */
@RunWith(AndroidJUnit4.class)
public final class CadPlanarFaceRuntimeTest {

    private static final String TAG = "ForgeShape";
    /**
     * A face row's AREA is exact (the arrangement integrates its arcs), so it
     * matches the analytic area closely.
     */
    private static final double CURVED_TOLERANCE = 0.01;
    /**
     * The SOLID is built from chords, and a chord lies inside a convex arc, so
     * a solid bounded by one is short of the exact area by a fraction of its
     * curved part. That fraction is larger for a small segment than for a
     * disk (1.03% for the 0.8 m lens on CI DEVICE 36790265291), and it is bounded
     * here at 2% of the curved part only, never of the whole body.
     */
    private static final double CHORD_DEFICIT_BOUND = 0.02;

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
            NativeViewport.supportChooserCancel();
            NativeViewport.sketchCancel();
            NativeViewport.enterConstructionMode();
            if (baselineProject != null) {
                NativeViewport.loadProject(baselineProject);
            }
            workspace.onNativeStateChanged();
            return null;
        });
        resetToBaselineConstruction(rule.getScenario());
    }

    // =======================================================================
    // J1: a circle crossing a rectangle offers three faces; the lens extrudes
    // =======================================================================

    @Test
    public void j1_crossing_circle_lens_new_body() {
        final int bodies = NativeViewport.sceneBodyCount();
        beginSketch(R.id.sketch_plane_xy);
        final Crossing c = drawSquareAndCrossingCircle(1.0, 1.5, 0.8);
        finishSketch();
        assertEquals("a crossing sketch selects planar faces", 1,
                NativeViewport.sketchSelectionKind());
        final long[] faces = faceHandles();
        assertEquals("rectangle rest, lens and the circle's outer part", 3, faces.length);
        for (long face : faces) {
            assertEquals("nothing is chosen for the user", 0.0,
                    info(face)[NativeViewport.SKETCH_REGION_SELECTED], 0.0);
        }
        final long lens = faceByRank(faces, 0);
        assertEquals("the lens is the analytic segment", c.lens,
                info(lens)[NativeViewport.SKETCH_REGION_AREA], c.lens * CURVED_TOLERANCE);

        tapFace(lens);
        assertEquals("the tap chose the lens", 1.0,
                info(lens)[NativeViewport.SKETCH_REGION_SELECTED], 0.0);
        assertEquals(1.0, toolState()[NativeViewport.CAD_EXTRUDE_SELECTED_REGIONS], 0.0);
        assertEquals("the lens candidate is valid", NativeViewport.CAD_OK,
                (int) toolState()[NativeViewport.CAD_EXTRUDE_CANDIDATE_STATUS]);

        final long body = extrudeWithDepth("0.5");
        assertTrue("a New Body", body != NativeViewport.NO_OBJECT);
        assertEquals(bodies + 1, NativeViewport.sceneBodyCount());
        assertEquals("the feature stores a PlanarFaces selection", 1,
                NativeViewport.cadFeatureSelectionKind(body, 0));
        final double[] m = measure(body);
        assertEquals("one shell", 1.0, m[NativeViewport.CAD_MEASURE_COMPONENTS], 0.0);
        assertChordedVolume("the lens, 0.5 deep", c.lens * 0.5, c.lens * 0.5, true,
                m[NativeViewport.CAD_MEASURE_VOLUME]);
        fact("j1.lens_volume_m3", m[NativeViewport.CAD_MEASURE_VOLUME]);
    }

    // =======================================================================
    // J2: every face together is the rectangle with a protrusion
    // =======================================================================

    @Test
    public void j2_every_face_is_a_protrusion() {
        beginSketch(R.id.sketch_plane_xy);
        final Crossing c = drawSquareAndCrossingCircle(1.0, 1.5, 0.8);
        finishSketch();
        final long[] faces = faceHandles();
        assertEquals(3, faces.length);
        tapFace(faceByRank(faces, 2));
        toggleFace(faceByRank(faces, 0));
        toggleFace(faceByRank(faces, 1));
        assertEquals(3.0, toolState()[NativeViewport.CAD_EXTRUDE_SELECTED_REGIONS], 0.0);
        assertEquals("the union is valid", NativeViewport.CAD_OK,
                (int) toolState()[NativeViewport.CAD_EXTRUDE_CANDIDATE_STATUS]);
        final long body = extrudeWithDepth("0.5");
        assertTrue(body != NativeViewport.NO_OBJECT);
        final double[] m = measure(body);
        final double expected = (c.rectangle + c.outside) * 0.5;
        assertEquals("one shell, no internal wall", 1.0,
                m[NativeViewport.CAD_MEASURE_COMPONENTS], 0.0);
        assertChordedVolume("rectangle plus bump", expected, c.outside * 0.5, true,
                m[NativeViewport.CAD_MEASURE_VOLUME]);
        fact("j2.protrusion_volume_m3", m[NativeViewport.CAD_MEASURE_VOLUME]);
    }

    // =======================================================================
    // J3: two overlapping circles
    // =======================================================================

    @Test
    public void j3_two_circles_lens() {
        beginSketch(R.id.sketch_plane_xy);
        selectTool(rule.getScenario(), R.id.tool_rail_circle);
        dragSketch(rule.getScenario(), -0.5, 0.0, 0.3, 0.0);
        final double[] a = applyCircleRadius(0.8);
        dragSketch(rule.getScenario(), 0.5, 0.0, 1.3, 0.0);
        final double[] b = applyCircleRadius(0.8);
        finishSketch();
        assertEquals(1, NativeViewport.sketchSelectionKind());
        final long[] faces = faceHandles();
        assertEquals("two crescents and a lens", 3, faces.length);
        final double d = Math.hypot(b[0] - a[0], b[1] - a[1]);
        final double lensArea = 2.0 * segment(0.8, d / 2.0);
        final long lens = faceByRank(faces, 0);
        assertEquals(lensArea, info(lens)[NativeViewport.SKETCH_REGION_AREA],
                lensArea * CURVED_TOLERANCE);
        tapFace(lens);
        final long body = extrudeWithDepth("0.5");
        assertTrue(body != NativeViewport.NO_OBJECT);
        assertEquals(1, NativeViewport.cadFeatureSelectionKind(body, 0));
        assertChordedVolume("the two-circle lens", lensArea * 0.5, lensArea * 0.5, true,
                measure(body)[NativeViewport.CAD_MEASURE_VOLUME]);
    }

    // =======================================================================
    // J4: two faces across one arc fragment union into one disk
    // =======================================================================

    @Test
    public void j4_multi_select_union_across_a_fragment() {
        beginSketch(R.id.sketch_plane_xy);
        drawSquareAndCrossingCircle(1.0, 1.5, 0.8);
        finishSketch();
        final long[] faces = faceHandles();
        tapFace(faceByRank(faces, 0));
        toggleFace(faceByRank(faces, 1));
        assertEquals(2.0, toolState()[NativeViewport.CAD_EXTRUDE_SELECTED_REGIONS], 0.0);
        final long body = extrudeWithDepth("0.5");
        assertTrue(body != NativeViewport.NO_OBJECT);
        final double disk = Math.PI * 0.8 * 0.8;
        final double[] m = measure(body);
        assertEquals("one shell: the shared piece is no wall", 1.0,
                m[NativeViewport.CAD_MEASURE_COMPONENTS], 0.0);
        assertChordedVolume("the whole disk", disk * 0.5, disk * 0.5, true,
                m[NativeViewport.CAD_MEASURE_VOLUME]);
    }

    // =======================================================================
    // J5: save, reopen, and reopen the feature on the same selection
    // =======================================================================

    @Test
    public void j5_save_reopen_and_edit_keep_the_selection() {
        beginSketch(R.id.sketch_plane_xy);
        drawSquareAndCrossingCircle(1.0, 1.5, 0.8);
        finishSketch();
        final long[] faces = faceHandles();
        tapFace(faceByRank(faces, 0));
        final long body = extrudeWithDepth("0.5");
        assertTrue(body != NativeViewport.NO_OBJECT);
        final double[] before = measure(body);
        final byte[] saved = NativeViewport.encodeProject();
        fact("j5.project_bytes", saved.length);

        final int loaded = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int status = NativeViewport.loadProject(saved);
            workspace.onNativeStateChanged();
            return status;
        });
        assertEquals("a PlanarFaces project opens", NativeViewport.PROJECT_OK, loaded);
        settleLayout();
        assertEquals(1, NativeViewport.cadFeatureSelectionKind(body, 0));
        final double[] after = measure(body);
        assertEquals("the same volume", before[NativeViewport.CAD_MEASURE_VOLUME],
                after[NativeViewport.CAD_MEASURE_VOLUME], 0.0);
        assertArrayEquals("re-encodes byte-identically", saved, NativeViewport.encodeProject());

        final int opened = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int status = NativeViewport.sketchBeginEditFeature(body, 1, true);
            workspace.onNativeStateChanged();
            return status;
        });
        assertEquals(NativeViewport.CAD_OK, opened);
        settleLayout();
        assertEquals(NativeViewport.SKETCH_READY, sketchState());
        assertEquals("the edit reopens in PlanarFaces", 1, NativeViewport.sketchSelectionKind());
        final long[] reopened = faceHandles();
        assertEquals(3, reopened.length);
        assertEquals("the lens is chosen again", 1.0,
                info(faceByRank(reopened, 0))[NativeViewport.SKETCH_REGION_SELECTED], 0.0);
        assertEquals("and nothing else", 0.0,
                info(faceByRank(reopened, 1))[NativeViewport.SKETCH_REGION_SELECTED]
                        + info(faceByRank(reopened, 2))[NativeViewport.SKETCH_REGION_SELECTED],
                0.0);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.sketchCancel();
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        assertArrayEquals("cancelling the edit costs nothing", saved,
                NativeViewport.encodeProject());
    }

    // =======================================================================
    // J6: a face selection on a body's cap feeds same-body Add, then Cut
    // =======================================================================

    @Test
    public void j6_face_selection_feeds_same_body_add_and_cut() {
        final long base = baseBodyOnXz();
        final int bodies = NativeViewport.sceneBodyCount();
        final double baseVolume = measure(base)[NativeViewport.CAD_MEASURE_VOLUME];

        final Crossing add = faceSketchSquareAndCrossingCircle();
        final long[] faces = faceHandles();
        tapFace(faceByRank(faces, 0));
        chooseOperationOnCanvas(R.id.cad_extrude_operation_add);
        assertEquals("the lens Add is valid", NativeViewport.CAD_OK,
                (int) toolState()[NativeViewport.CAD_EXTRUDE_CANDIDATE_STATUS]);
        assertEquals("Add returns the SAME body", base, extrudeWithDepth("0.5"));
        assertEquals(bodies, NativeViewport.sceneBodyCount());
        assertEquals("the second feature stores PlanarFaces", 1,
                NativeViewport.cadFeatureSelectionKind(base, 1));
        assertChordedVolume("base plus the lens", baseVolume + add.lens * 0.5, add.lens * 0.5,
                true, measure(base)[NativeViewport.CAD_MEASURE_VOLUME]);
        undo();
        assertEquals("one Undo removes exactly the Add", baseVolume,
                measure(base)[NativeViewport.CAD_MEASURE_VOLUME], baseVolume * 1e-4);

        final Crossing cut = faceSketchSquareAndCrossingCircle();
        tapFace(faceByRank(faceHandles(), 0));
        chooseOperationOnCanvas(R.id.cad_extrude_operation_cut);
        assertEquals("the lens Cut is valid", NativeViewport.CAD_OK,
                (int) toolState()[NativeViewport.CAD_EXTRUDE_CANDIDATE_STATUS]);
        assertEquals("Cut returns the SAME body", base, extrudeWithDepth("0.5"));
        final double[] m = measure(base);
        assertEquals("one shell", 1.0, m[NativeViewport.CAD_MEASURE_COMPONENTS], 0.0);
        assertChordedVolume("base minus the lens", baseVolume - cut.lens * 0.5, cut.lens * 0.5,
                false, m[NativeViewport.CAD_MEASURE_VOLUME]);
    }

    // -----------------------------------------------------------------------
    // Sketching
    // -----------------------------------------------------------------------

    /** The analytic areas of a square crossed on its +u edge by a circle. */
    private static final class Crossing {
        double lens;
        double outside;
        double rectangle;
    }

    /**
     * A centred square of half side {@code half}, and a circle of radius
     * {@code radius} centred at {@code (cu, 0)} across its +u edge. What the
     * grid snapped is read back, and the analytic areas follow from it.
     */
    private Crossing drawSquareAndCrossingCircle(double half, double cu, double radius) {
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), -half, -half, half, half);
        applyRectangleSize(2.0 * half);
        final double[] rect = selectedEntityValues();
        selectTool(rule.getScenario(), R.id.tool_rail_circle);
        dragSketch(rule.getScenario(), cu, 0.0, cu + radius, 0.0);
        final double[] circle = applyCircleRadius(radius);
        return crossing(rect, circle, radius);
    }

    private static Crossing crossing(double[] rect, double[] circle, double radius) {
        final double right = rect[0] + rect[2] / 2.0;
        assertTrue("the circle crosses the +u edge from outside",
                circle[0] > right && circle[0] - right < radius);
        assertTrue("and stays clear of the other edges",
                Math.abs(circle[1] - rect[1]) + radius < rect[3] / 2.0);
        final Crossing c = new Crossing();
        c.lens = segment(radius, circle[0] - right);
        c.outside = Math.PI * radius * radius - c.lens;
        c.rectangle = rect[2] * rect[3];
        return c;
    }

    /** The area of a circle's segment cut off by a chord at distance {@code d}. */
    private static double segment(double r, double d) {
        return r * r * Math.acos(d / r) - d * Math.sqrt(r * r - d * d);
    }

    /** {centre u, centre v, width, height} of the selected rectangle. */
    private static double[] selectedEntityValues() {
        final double[] drawn = new double[NativeViewport.SKETCH_ENTITY_SIZE];
        assertTrue(NativeViewport.sketchSelectedEntity(drawn));
        return Arrays.copyOfRange(drawn, NativeViewport.SKETCH_ENTITY_VALUES,
                NativeViewport.SKETCH_ENTITY_SIZE);
    }

    /** Types the exact size into the just-drawn square. */
    private void applyRectangleSize(final double side) {
        final double[] drawn = new double[NativeViewport.SKETCH_ENTITY_SIZE];
        assertTrue(NativeViewport.sketchSelectedEntity(drawn));
        assertEquals(NativeViewport.SKETCH_ENTITY_KIND_RECTANGLE,
                (int) drawn[NativeViewport.SKETCH_ENTITY_KIND]);
        final long id = (long) drawn[NativeViewport.SKETCH_ENTITY_ID];
        final int status = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int s = NativeViewport.sketchApplyRectangle(id, side, side);
            workspace.onNativeStateChanged();
            return s;
        });
        assertEquals(NativeViewport.CAD_OK, status);
        settleLayout();
    }

    /** Types the exact radius into the just-drawn circle; returns its centre. */
    private double[] applyCircleRadius(final double radius) {
        final double[] drawn = new double[NativeViewport.SKETCH_ENTITY_SIZE];
        assertTrue(NativeViewport.sketchSelectedEntity(drawn));
        assertEquals(NativeViewport.SKETCH_ENTITY_KIND_CIRCLE,
                (int) drawn[NativeViewport.SKETCH_ENTITY_KIND]);
        final long id = (long) drawn[NativeViewport.SKETCH_ENTITY_ID];
        final int status = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int s = NativeViewport.sketchApplyCircle(id, radius);
            workspace.onNativeStateChanged();
            return s;
        });
        assertEquals(NativeViewport.CAD_OK, status);
        settleLayout();
        return new double[]{drawn[NativeViewport.SKETCH_ENTITY_VALUES],
                drawn[NativeViewport.SKETCH_ENTITY_VALUES + 1]};
    }

    private void beginSketch(final int planeId) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openAddPrimitive(workspace);
            workspace.addPrimitivePalette().findViewById(R.id.sketch_plane_by_name).performClick();
            workspace.addPrimitivePalette().findViewById(planeId).performClick();
            return null;
        });
        settleLayout();
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
    }

    private void finishSketch() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.finish_sketch).performClick();
            return null;
        });
        settleLayout();
        assertEquals(NativeViewport.SKETCH_READY, sketchState());
    }

    private long baseBodyOnXz() {
        beginSketch(R.id.sketch_plane_xz);
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), -1.0, -1.0, 1.0, 1.0);
        finishSketch();
        final long base = extrudeWithDepth("1");
        assertTrue("a base body", base != NativeViewport.NO_OBJECT);
        return base;
    }

    /**
     * Opens a sketch on the XZ base's top cap through the spatial chooser and
     * draws a 1.2 m square crossed on its +u edge by a 0.45 m circle, all of it
     * on the cap's material except the circle's outer part.
     */
    private Crossing faceSketchSquareAndCrossingCircle() {
        assertTrue(NativeViewport.debugSetCameraPose(0.7f, 0.9f, 8.0f));
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(NativeViewport.supportChooserBegin(true));
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        tapWorld(rule.getScenario(), 0.0, 1.0, 0.0);
        tapWorld(rule.getScenario(), 0.0, 1.0, 0.0);
        assertEquals("a face-supported sketch is open", NativeViewport.SKETCH_EDITING,
                sketchState());
        final Crossing c = drawSquareAndCrossingCircle(0.6, 0.85, 0.45);
        finishSketch();
        assertEquals(1, NativeViewport.sketchSelectionKind());
        assertEquals(3, faceHandles().length);
        return c;
    }

    private long extrudeWithDepth(final String depth) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final EditText field = workspace.sketchEditor().findViewById(R.id.field_extrude_depth);
            field.setText(depth);
            workspace.findViewById(R.id.sketch_extrude_commit).performClick();
            if (sketchState() != NativeViewport.SKETCH_INACTIVE) {
                return NativeViewport.NO_OBJECT;
            }
            return NativeViewport.sceneActiveBodyId();
        });
    }

    private void chooseOperationOnCanvas(int optionId) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View canvas = workspace.cadExtrudeCanvas();
            canvas.findViewById(R.id.cad_extrude_panel).performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View option = workspace.cadExtrudeCanvas().findViewById(optionId);
            assertNotNull(option);
            assertTrue("the option is offered", option.isShown());
            option.performClick();
            return null;
        });
        settleLayout();
    }

    private void undo() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.constructionUndo();
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    // -----------------------------------------------------------------------
    // Faces
    // -----------------------------------------------------------------------

    private static long[] faceHandles() {
        final int count = NativeViewport.sketchProfiles(null);
        final long[] handles = new long[Math.max(count, 0)];
        assertEquals(count, NativeViewport.sketchProfiles(handles));
        return handles;
    }

    private static double[] info(long handle) {
        final double[] out = new double[NativeViewport.SKETCH_REGION_INFO_SIZE];
        assertTrue(NativeViewport.sketchProfileInfo(handle, out));
        return out;
    }

    /** The face of the given rank by ascending area (0 = smallest). */
    private static long faceByRank(long[] handles, int rank) {
        final Long[] sorted = new Long[handles.length];
        for (int i = 0; i < handles.length; ++i) {
            sorted[i] = handles[i];
        }
        Arrays.sort(sorted, (x, y) -> Double.compare(info(x)[NativeViewport.SKETCH_REGION_AREA],
                info(y)[NativeViewport.SKETCH_REGION_AREA]));
        return sorted[rank];
    }

    /** A real touch on the face's own interior point, projected by native. */
    private void tapFace(long handle) {
        final double[] at = info(handle);
        assertEquals("the face projects on screen", 1.0,
                at[NativeViewport.SKETCH_REGION_ON_SCREEN], 0.0);
        tapViewport(rule.getScenario(), (float) at[NativeViewport.SKETCH_REGION_SCREEN_X],
                (float) at[NativeViewport.SKETCH_REGION_SCREEN_Y]);
        assertEquals("the tap chose the face", 1.0,
                info(handle)[NativeViewport.SKETCH_REGION_SELECTED], 0.0);
    }

    /**
     * Adds a face through the precision surface's row path, which a later
     * pick uses so a tap can never land on the extrude arrow a first pick drew.
     */
    private void toggleFace(long handle) {
        final int status = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int s = NativeViewport.sketchToggleRegion(handle);
            workspace.onNativeStateChanged();
            return s;
        });
        assertEquals(NativeViewport.CAD_OK, status);
        settleLayout();
        assertEquals(1.0, info(handle)[NativeViewport.SKETCH_REGION_SELECTED], 0.0);
    }

    /**
     * {@code actual} differs from the exact {@code expected} only by the chord
     * deficit of the curved part: on the short side when that part is
     * material ({@code materialIsCurved}), on the long side when it was cut
     * away, and by no more than {@link #CHORD_DEFICIT_BOUND} of it.
     */
    private static void assertChordedVolume(String what, double expected, double curvedPart,
                                            boolean materialIsCurved, double actual) {
        final double deficit = materialIsCurved ? expected - actual : actual - expected;
        fact("chord_deficit." + what.replace(' ', '_'), deficit / curvedPart);
        assertTrue(what + ": chords lie inside the arc, so never past the exact volume ("
                + actual + " vs " + expected + ")", deficit >= -1e-9 * Math.abs(expected));
        assertTrue(what + ": within the chord bound (" + actual + " vs " + expected + ")",
                deficit <= CHORD_DEFICIT_BOUND * curvedPart);
    }

    private static double[] toolState() {
        final double[] tool = new double[NativeViewport.CAD_EXTRUDE_SIZE];
        NativeViewport.cadExtrudeToolState(tool);
        return tool;
    }

    private static double[] measure(long body) {
        final double[] out = new double[NativeViewport.CAD_MEASURE_SIZE];
        assertTrue("the body measures as a CAD solid", NativeViewport.cadBodyMeasure(body, out));
        return out;
    }

    private static void fact(String key, Object value) {
        Log.i(TAG, "CADV6S2_DEVICE " + key + "=" + value);
    }
}
