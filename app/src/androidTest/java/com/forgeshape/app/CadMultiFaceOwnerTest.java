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
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.os.SystemClock;
import android.util.Log;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
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
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * `CAD-V6-S2-OWNER-FEEDBACK-MULTIFACE-E2E-R1` on the device, on the OWNER's own
 * path: Home → New Project → CAD, a sketch of 24 cells, Finish, then REAL
 * window taps. A fill selection crosses the old 16-area cap, holds every cell,
 * survives removal and re-adding in another order, and the first Extrude of a
 * new project commits it — while the status line always names the CURRENT
 * state and never says "Several profiles are closed — choose one" over a
 * non-empty selection.
 *
 * <p>Asserted from native truth (face handles, tool state, the candidate and
 * body measures, the debug tap attribution tokens) and from the status line's
 * own text, never from a picture.
 */
@RunWith(AndroidJUnit4.class)
public final class CadMultiFaceOwnerTest {

    private static final String TAG = "ForgeShape";
    private static final int COLS = 6;
    private static final int ROWS = 4;
    private static final int CELLS = COLS * ROWS;

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private File outDir;
    private final List<String> facts = new ArrayList<>();
    /** Cell size in metres (two drawing units). */
    private double cell;
    /** Face handle per cell, reading order: row by row from the lower left. */
    private long[] handles;
    /** The status line the moment Finish returned. */
    private String finishStatus = "";
    private int marks;

    @Before
    public void startAtHome() {
        outDir = new File(InstrumentationRegistry.getInstrumentation().getTargetContext()
                .getExternalFilesDir(null), "evidence/cad-v6-s2-owner-feedback-multiface");
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
    // DEV-MF-01: real taps 1..18 cross the old cap one by one
    // =======================================================================

    @Test
    public void devMf01_real_taps_cross_the_old_sixteen_area_cap() {
        newCadProjectGrid();
        final String noChoice = onWorkspace(rule.getScenario(),
                (activity, workspace) -> activity.getString(R.string.status_sketch_finished_tap_region,
                        CELLS));
        assertEquals("Finish with nothing chosen says so", noChoice, finishStatus);
        for (int k = 0; k < 18; k++) {
            final String mark = mark();
            tapCell(k);
            final List<String> tokens = tokensSince(mark);
            final long[] now = selected();
            fact("mf01.tap" + (k + 1), "count=" + now.length + " tokens=" + tokens + " status="
                    + statusLine());
            assertEquals("tap " + (k + 1) + " adds its cell", k + 1, now.length);
            assertTrue("tap " + (k + 1) + " resolved: " + tokens,
                    tokens.contains("FORGESHAPE_SKETCH_TAP:resolved"));
            assertFalse("no selection cap at tap " + (k + 1) + ": " + tokens,
                    tokens.contains("FORGESHAPE_SKETCH_TAP:selection_cap"));
            assertEquals(NativeViewport.CAD_OK, NativeViewport.sketchLastStatus());
            assertEquals("the status names the count", regionsSelected(k + 1), statusLine());
            assertNoChooseOne("tap " + (k + 1));
            assertEquals("the candidate stays valid", NativeViewport.CAD_OK, candidateStatus());
        }
        assertEquals(17 + 1, selected().length);
    }

    // =======================================================================
    // DEV-MF-02: every one of the 24 cells, exact count, current preview
    // =======================================================================

    @Test
    public void devMf02_all_twenty_four_cells_select_and_the_preview_follows() {
        newCadProjectGrid();
        for (int k = 0; k < CELLS; k++) {
            tapCell(k);
        }
        final long[] all = selected();
        assertEquals("every cell is chosen", CELLS, all.length);
        final long[] sortedHandles = handles.clone();
        Arrays.sort(sortedHandles);
        assertArrayEquals("exactly the grid's cells", sortedHandles, all);
        assertEquals(regionsSelected(CELLS), statusLine());
        final double[] preview = candidateMeasure();
        final double depth = depth();
        final double area = selectedArea();
        fact("mf02.preview", "volume=" + preview[0] + " components=" + preview[1] + " depth=" + depth
                + " area=" + area);
        assertEquals("the cells are the grid drawn", CELLS * cell * cell, area, 1e-9);
        assertEquals("the preview is the whole slab", area * depth, preview[0], 1e-6 * area * depth);
        assertEquals("one component", 1.0, preview[1], 0.0);
        assertTrue("the toolbar's Extrude is offered", extrudeShown());
    }

    // =======================================================================
    // DEV-MF-03: remove the cells at the old boundary, re-add in another order
    // =======================================================================

    @Test
    public void devMf03_removing_and_readding_cells_around_the_old_cap_restores_the_set() {
        newCadProjectGrid();
        for (int k = 0; k < 18; k++) {
            tapCell(k);
        }
        final long[] before = selected();
        assertEquals(18, before.length);
        for (int k : new int[]{14, 15, 16, 17}) {
            tapCell(k);
        }
        assertEquals("four removed", 14, selected().length);
        assertEquals(regionsSelected(14), statusLine());
        for (int k : new int[]{17, 14, 16, 15}) {
            tapCell(k);
        }
        assertArrayEquals("the exact set is back", before, selected());
        assertEquals(regionsSelected(18), statusLine());
        assertNoChooseOne("after re-adding");
    }

    // =======================================================================
    // DEV-MF-04: the first Extrude of a new project commits 20 cells
    // =======================================================================

    @Test
    public void devMf04_the_first_extrude_commits_a_twenty_cell_selection() {
        newCadProjectGrid();
        // Rows 0..2 whole and two cells of row 3: one edge-connected block.
        for (int k = 0; k < 20; k++) {
            tapCell(k);
        }
        assertEquals(20, selected().length);
        final double[] preview = candidateMeasure();
        final double depth = depth();
        final double area = selectedArea();
        assertEquals("the block is the twenty cells drawn", 20 * cell * cell, area, 1e-9);
        final double expected = area * depth;
        assertEquals("the preview is the block", expected, preview[0], 1e-6 * expected);
        assertEquals(1.0, preview[1], 0.0);
        assertTrue("the toolbar's Extrude is offered", extrudeShown());
        assertFalse("no project exists before the first Extrude", NativeViewport.projectOpen());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.extrude_sketch).performClick();
            return null;
        });
        settleLayout();
        fact("mf04.after_extrude", "state=" + sketchState() + " status=" + statusLine()
                + " last=" + NativeViewport.sketchLastStatus());
        assertEquals("the sketch is committed: " + statusLine(), NativeViewport.SKETCH_INACTIVE,
                sketchState());
        assertTrue("the first Extrude created the project", NativeViewport.projectOpen());
        assertEquals(1, NativeViewport.sceneBodyCount());
        assertTrue(NativeViewport.sceneActiveBodyIsCad());
        final double[] body = new double[NativeViewport.CAD_MEASURE_SIZE];
        assertTrue(NativeViewport.cadBodyMeasure(NativeViewport.sceneActiveBodyId(), body));
        fact("mf04.body", "volume=" + body[NativeViewport.CAD_MEASURE_VOLUME] + " components="
                + body[NativeViewport.CAD_MEASURE_COMPONENTS]);
        assertEquals("the body is the block", expected, body[NativeViewport.CAD_MEASURE_VOLUME],
                1e-6 * expected);
        assertEquals(1.0, body[NativeViewport.CAD_MEASURE_COMPONENTS], 0.0);
        assertEquals("a planar-face feature", 1, NativeViewport.cadFeatureSelectionKind(
                NativeViewport.sceneActiveBodyId(), 0));
        assertNoChooseOne("after the commit");
    }

    // =======================================================================
    // DEV-MF-05: the status line always describes the current state
    // =======================================================================

    @Test
    public void devMf05_the_status_line_is_always_the_current_verdict() {
        newCadProjectGrid();
        tapCell(0);
        assertEquals(regionsSelected(1), statusLine());
        tapCell(0);
        // Seventeen cells whose union pinches at the corner between (row 1,
        // col 1) and (row 2, col 2) -- the pattern the host's MF_15C pins.
        final int[][] pinch = {{0, 1}, {1, 1}, {0, 2}, {2, 2}, {0, 3}, {1, 3}, {2, 3}, {3, 0},
                {3, 1}, {3, 2}, {3, 3}, {3, 4}, {3, 5}, {0, 5}, {1, 5}, {2, 5}, {0, 0}};
        for (int[] rc : pinch) {
            tapCell(rc[0] * COLS + rc[1]);
        }
        assertEquals(17, selected().length);
        final String touch = onWorkspace(rule.getScenario(),
                (activity, workspace) -> activity.getString(R.string.status_cad_areas_touch_at_point));
        assertEquals("the candidate's own refusal", NativeViewport.CAD_PLANAR_FACES_TOUCH_AT_POINT,
                candidateStatus());
        assertEquals("the status names it", touch, statusLine());
        assertFalse("a refused candidate withdraws the toolbar's Extrude", extrudeShown());
        // The precision surface's Extrude still submits: the commit is refused
        // by the CURRENT reason, never by a selection message.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final android.widget.EditText field =
                    workspace.sketchEditor().findViewById(R.id.field_extrude_depth);
            field.setText("1");
            workspace.findViewById(R.id.sketch_extrude_commit).performClick();
            return null;
        });
        settleLayout();
        fact("mf05.refused_commit", "last=" + NativeViewport.sketchLastStatus() + " status="
                + statusLine());
        assertEquals(NativeViewport.SKETCH_READY, sketchState());
        assertFalse(NativeViewport.projectOpen());
        assertEquals(NativeViewport.CAD_PLANAR_FACES_TOUCH_AT_POINT, NativeViewport.sketchLastStatus());
        assertEquals("the refused commit reports its own reason", touch, statusLine());
        assertNoChooseOne("after the refused commit");
        // Drop (row 2, col 2): valid again, sixteen cells, the count is back.
        tapCell(2 * COLS + 2);
        assertEquals(16, selected().length);
        assertEquals(NativeViewport.CAD_OK, candidateStatus());
        assertEquals("the stale refusal is gone", regionsSelected(16), statusLine());
        assertTrue(extrudeShown());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.extrude_sketch).performClick();
            return null;
        });
        settleLayout();
        assertEquals("the fixed selection commits: " + statusLine(), NativeViewport.SKETCH_INACTIVE,
                sketchState());
        assertTrue(NativeViewport.projectOpen());
    }

    // =======================================================================
    // DEV-MF-06: past the old cap, a jittered tap from the far side still
    // picks its cell without orbiting
    // =======================================================================

    @Test
    public void devMf06_past_the_cap_a_jittered_tap_from_below_the_plane_picks_without_orbit() {
        newCadProjectGrid();
        for (int k = 0; k < 18; k++) {
            tapCell(k);
        }
        // Below the XY plane: a negative pitch puts the eye at -z.
        setCamera(0.4f, -0.6f, 9.0f * (float) (cell / 0.4));
        final float[] at = tappablePoint(18);
        assertNotNull("cell 19 is reachable from below: " + lastBlock, at);
        final float[] poseBefore = cameraPose();
        final String mark = mark();
        realGesture(new float[][]{{at[0], at[1]}, {at[0] + 4.0f, at[1] - 3.0f},
                {at[0] + 8.0f, at[1] + 6.0f}});
        final List<String> tokens = tokensSince(mark);
        fact("mf06.tap", Arrays.toString(at) + " tokens=" + tokens);
        assertArrayEquals("no orbit inside the slop", poseBefore, cameraPose(), 0.0f);
        assertEquals("the 19th cell joined", 19, selected().length);
        assertTrue(Arrays.binarySearch(selected(), handles[18]) >= 0);
        assertTrue(tokens.contains("FORGESHAPE_SKETCH_TAP:resolved"));
        assertEquals(regionsSelected(19), statusLine());
    }

    // -----------------------------------------------------------------------
    // The sketch: New Project -> CAD, a 6 x 4 grid of cells, Finish
    // -----------------------------------------------------------------------

    private void newCadProjectGrid() {
        press(R.id.home_new_project);
        press(R.id.new_project_cad);
        assertEquals("New CAD lands in a sketch", NativeViewport.SKETCH_EDITING, sketchState());
        assertFalse(NativeViewport.projectOpen());
        // Each entity is placed by a real drag and then TYPED to its exact
        // geometry through the precision path the Sketch values surface uses
        // (`sketchApply…`): a drag snaps to the view's touch grid, which need
        // not be the step `sketchGridStep` reports, and the grid under test
        // must be exact. The taps that follow are what this class proves.
        settleLayout();
        cell = 0.4;
        final double hu = 0.5 * COLS * cell;
        final double hv = 0.5 * ROWS * cell;
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), -hu, -hv, hu, hv);
        final double[] drawn = new double[NativeViewport.SKETCH_ENTITY_SIZE];
        assertTrue("the new rectangle is selected", NativeViewport.sketchSelectedEntity(drawn));
        assertEquals(NativeViewport.SKETCH_ENTITY_KIND_RECTANGLE,
                (int) drawn[NativeViewport.SKETCH_ENTITY_KIND]);
        fact("rectangle.dragged", Arrays.toString(drawn));
        final long rectangle = (long) drawn[NativeViewport.SKETCH_ENTITY_ID];
        assertEquals(NativeViewport.CAD_OK, applyOnUi(() ->
                NativeViewport.sketchApplyRectangle(rectangle, 2 * hu, 2 * hv)));
        assertTrue(NativeViewport.sketchSelectedEntity(drawn));
        assertEquals("centred", 0.0, drawn[NativeViewport.SKETCH_ENTITY_VALUES], 1e-9);
        assertEquals("centred", 0.0, drawn[NativeViewport.SKETCH_ENTITY_VALUES + 1], 1e-9);
        assertEquals("the rectangle is exact (width)", 2 * hu,
                drawn[NativeViewport.SKETCH_ENTITY_VALUES + 2], 1e-9);
        assertEquals("the rectangle is exact (height)", 2 * hv,
                drawn[NativeViewport.SKETCH_ENTITY_VALUES + 3], 1e-9);
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        for (int k = 1; k < COLS; k++) {
            placeLine(-hu + k * cell, -hv, -hu + k * cell, hv);
        }
        for (int k = 1; k < ROWS; k++) {
            placeLine(-hu, -hv + k * cell, hu, -hv + k * cell);
        }
        assertEquals("the rectangle and its grid lines", 1 + (COLS - 1) + (ROWS - 1),
                sketchEntityCount());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.finish_sketch).performClick();
            return null;
        });
        settleLayout();
        assertEquals("Finish succeeds: " + NativeViewport.sketchLastStatus(),
                NativeViewport.SKETCH_READY, sketchState());
        assertEquals("fill mode", 1, NativeViewport.sketchSelectionKind());
        assertEquals("twenty-four cells", CELLS, NativeViewport.sketchProfiles(null));
        assertEquals("nothing chosen for the user", 0, selected().length);
        // Read before the camera moves: a re-sync rests the status line.
        finishStatus = statusLine();
        // A view that sees every cell clearly, oblique, above the plane.
        setCamera(0.35f, 0.9f, 8.0f * (float) (cell / 0.4));
        handles = new long[CELLS];
        final long[] faces = faceHandles();
        for (int k = 0; k < CELLS; k++) {
            final double u = -hu + (k % COLS) * cell;
            final double v = -hv + (k / COLS) * cell;
            handles[k] = faceAt(faces, u, v, u + cell, v + cell);
            assertTrue("cell " + k + " resolves to a face", handles[k] > 0);
        }
        final long[] distinct = handles.clone();
        Arrays.sort(distinct);
        for (int k = 1; k < CELLS; k++) {
            assertTrue("cells are distinct faces", distinct[k] != distinct[k - 1]);
        }
        fact("grid", "cell=" + cell + " handles=" + Arrays.toString(handles));
    }

    /** A line by a real drag, then typed to its exact endpoints. */
    private void placeLine(double u0, double v0, double u1, double v1) {
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
    }

    /** A native typed-value apply, then the workspace's re-read, as the panel does. */
    private int applyOnUi(java.util.function.IntSupplier apply) {
        final int status = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int why = apply.getAsInt();
            workspace.onNativeStateChanged();
            return why;
        });
        settleLayout();
        return status;
    }

    /**
     * The face handle whose native interior point lies inside the cell
     * [u0, u1] x [v0, v1], decided on screen: the cell's four corners are
     * projected and the interior point tested against that convex quad, so no
     * assumption about where inside its face native puts the point is made.
     */
    private static long faceAt(long[] faces, double u0, double v0, double u1, double v1) {
        final double[][] corners = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};
        final float[][] quad = new float[4][2];
        for (int i = 0; i < 4; i++) {
            assertTrue(NativeViewport.sketchScreenPoint(corners[i][0], corners[i][1], quad[i]));
        }
        final double[] info = new double[NativeViewport.SKETCH_REGION_INFO_SIZE];
        long found = -1;
        for (long handle : faces) {
            assertTrue(NativeViewport.sketchProfileInfo(handle, info));
            if (info[NativeViewport.SKETCH_REGION_ON_SCREEN] == 0.0) continue;
            final double x = info[NativeViewport.SKETCH_REGION_SCREEN_X];
            final double y = info[NativeViewport.SKETCH_REGION_SCREEN_Y];
            int positive = 0;
            int negative = 0;
            for (int i = 0; i < 4; i++) {
                final float[] a = quad[i];
                final float[] b = quad[(i + 1) % 4];
                final double cross = (b[0] - a[0]) * (y - a[1]) - (b[1] - a[1]) * (x - a[0]);
                if (cross > 0) positive++;
                if (cross < 0) negative++;
            }
            if (positive == 4 || negative == 4) {
                assertEquals("one face per cell", -1, found);
                found = handle;
            }
        }
        return found;
    }

    // -----------------------------------------------------------------------
    // Taps
    // -----------------------------------------------------------------------

    /** A point of cell k a finger can reach: its centre, then four inset points. */
    private float[] tappablePoint(int k) {
        final double hu = 0.5 * COLS * cell;
        final double hv = 0.5 * ROWS * cell;
        final double cu = -hu + (k % COLS + 0.5) * cell;
        final double cv = -hv + (k / COLS + 0.5) * cell;
        final double d = 0.3 * cell;
        final double[][] candidates = {{cu, cv}, {cu - d, cv - d}, {cu + d, cv - d},
                {cu - d, cv + d}, {cu + d, cv + d}};
        for (boolean avoidArrow : new boolean[]{true, false}) {
            for (double[] p : candidates) {
                final float[] at = new float[2];
                if (!NativeViewport.sketchScreenPoint(p[0], p[1], at)) continue;
                if (avoidArrow && shaftDistance(at[0], at[1]) < 48.0f) continue;
                if (firstViewportPoint(at) != null) return at;
            }
        }
        return null;
    }

    /** One still real tap on cell k; asserts exactly that cell toggled. */
    private void tapCell(int k) {
        final long[] before = selected();
        final float[] at = tappablePoint(k);
        assertNotNull("cell " + k + " has a tappable point: " + lastBlock, at);
        realGesture(new float[][]{{at[0], at[1]}});
        final long[] after = selected();
        final boolean wasOn = Arrays.binarySearch(before, handles[k]) >= 0;
        assertEquals("cell " + k + " toggled (last status " + NativeViewport.sketchLastStatus()
                + ", status '" + statusLine() + "')", before.length + (wasOn ? -1 : 1), after.length);
        assertEquals("and it is cell " + k, !wasOn, Arrays.binarySearch(after, handles[k]) >= 0);
    }

    private String lastBlock = "";

    /** The point when it is on the viewport and no clickable chrome stands over it. */
    private float[] firstViewportPoint(float[] at) {
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
        if (view instanceof android.view.ViewGroup) {
            final android.view.ViewGroup group = (android.view.ViewGroup) view;
            for (int i = group.getChildCount() - 1; i >= 0; i--) {
                final View hit = clickableAt(group.getChildAt(i), wx, wy);
                if (hit != null) {
                    return hit;
                }
            }
        }
        return view.isClickable() || HUD_DOWN_TAKERS.contains(view.getId())
                || view.getId() == R.id.viewport_surface ? view : null;
    }

    /**
     * The canvas HUD views that consume a Down over the viewport whether or
     * not they are marked clickable (the debug `FORGESHAPE_CAD_HUD_TOUCH`
     * reasons): a finger on one of them is the HUD's, never a cell's.
     */
    private static final java.util.Set<Integer> HUD_DOWN_TAKERS = new java.util.HashSet<>(
            Arrays.asList(R.id.cad_extrude_depth_value, R.id.cad_extrude_second_value,
                    R.id.cad_extrude_depth_editor, R.id.cad_extrude_second_editor,
                    R.id.cad_extrude_panel, R.id.cad_canvas_edit_sketch));

    /** px from the drawn arrow (base to its point); infinite with no arrow. */
    private static float shaftDistance(float x, float y) {
        final double[] tool = toolState();
        if (tool[NativeViewport.CAD_EXTRUDE_ACTIVE] == 0.0
                || tool[NativeViewport.CAD_EXTRUDE_ON_SCREEN] == 0.0) {
            return Float.POSITIVE_INFINITY;
        }
        final float tx = (float) tool[NativeViewport.CAD_EXTRUDE_TIP_X];
        final float ty = (float) tool[NativeViewport.CAD_EXTRUDE_TIP_Y];
        final float bx = 2.0f * (float) tool[NativeViewport.CAD_EXTRUDE_LABEL_X] - tx;
        final float by = 2.0f * (float) tool[NativeViewport.CAD_EXTRUDE_LABEL_Y] - ty;
        float ex = tx;
        float ey = ty;
        if (tool[NativeViewport.CAD_EXTRUDE_HEAD_ON_SCREEN] != 0.0) {
            ex = (float) tool[NativeViewport.CAD_EXTRUDE_HEAD_X];
            ey = (float) tool[NativeViewport.CAD_EXTRUDE_HEAD_Y];
        }
        final float dx = ex - bx;
        final float dy = ey - by;
        final float len2 = dx * dx + dy * dy;
        float t = len2 > 0.0f ? ((x - bx) * dx + (y - by) * dy) / len2 : 0.0f;
        t = Math.max(0.0f, Math.min(1.0f, t));
        return (float) Math.hypot(x - (bx + dx * t), y - (by + dy * t));
    }

    /**
     * One finger through the WINDOW: Down at the first point, a Move to each
     * later one, Up at the last -- whatever stands over the viewport receives
     * it first, as from a real touch screen.
     */
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

    private void setCamera(float yaw, float pitch, float distance) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(NativeViewport.debugSetCameraPose(yaw, pitch, distance));
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    private static float[] cameraPose() {
        final float[] pose = new float[3];
        NativeViewport.debugCameraPose(pose);
        return pose;
    }

    private boolean extrudeShown() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.findViewById(R.id.extrude_sketch).isShown());
    }

    private static double depth() {
        final double[] state = new double[NativeViewport.SKETCH_STATE_SIZE];
        NativeViewport.sketchState(state);
        return state[NativeViewport.SKETCH_EXTRUDE_DEPTH];
    }

    private static long[] faceHandles() {
        final int count = NativeViewport.sketchProfiles(null);
        final long[] out = new long[Math.max(count, 0)];
        assertEquals(count, NativeViewport.sketchProfiles(out));
        return out;
    }

    /** The selected face handles, ascending. */
    private static long[] selected() {
        final List<Long> out = new ArrayList<>();
        final double[] info = new double[NativeViewport.SKETCH_REGION_INFO_SIZE];
        for (long handle : faceHandles()) {
            assertTrue(NativeViewport.sketchProfileInfo(handle, info));
            if (info[NativeViewport.SKETCH_REGION_SELECTED] != 0.0) {
                out.add(handle);
            }
        }
        final long[] result = new long[out.size()];
        for (int i = 0; i < result.length; i++) {
            result[i] = out.get(i);
        }
        Arrays.sort(result);
        return result;
    }

    /** The summed native area of the selected faces, square metres. */
    private static double selectedArea() {
        double sum = 0.0;
        final double[] info = new double[NativeViewport.SKETCH_REGION_INFO_SIZE];
        for (long handle : selected()) {
            assertTrue(NativeViewport.sketchProfileInfo(handle, info));
            sum += info[NativeViewport.SKETCH_REGION_AREA];
        }
        return sum;
    }

    private static double[] toolState() {
        final double[] tool = new double[NativeViewport.CAD_EXTRUDE_SIZE];
        NativeViewport.cadExtrudeToolState(tool);
        return tool;
    }

    private static int candidateStatus() {
        return (int) toolState()[NativeViewport.CAD_EXTRUDE_CANDIDATE_STATUS];
    }

    private static double[] candidateMeasure() {
        final double[] out = new double[NativeViewport.CANDIDATE_MEASURE_SIZE];
        assertTrue("the candidate measures", NativeViewport.sketchCandidateMeasure(out));
        assertEquals("the candidate is valid", NativeViewport.CAD_OK,
                (int) out[NativeViewport.CANDIDATE_MEASURE_STATUS]);
        return new double[]{out[NativeViewport.CANDIDATE_MEASURE_VOLUME],
                out[NativeViewport.CANDIDATE_MEASURE_COMPONENTS]};
    }

    private String statusLine() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View message = workspace.findViewById(R.id.status_message);
            return message instanceof TextView && message.isShown()
                    ? ((TextView) message).getText().toString() : "";
        });
    }

    private String regionsSelected(int count) {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> activity.getString(R.string.status_regions_selected, count));
    }

    /** Over a non-empty selection the status line never says "choose one". */
    private void assertNoChooseOne(String where) {
        final String chooseOne = onWorkspace(rule.getScenario(),
                (activity, workspace) -> activity.getString(R.string.status_cad_ambiguous_profile));
        final String line = statusLine();
        assertFalse(where + ": the status line says choose one: " + line, line.equals(chooseOne));
        assertTrue(where + ": no AmbiguousProfile",
                NativeViewport.sketchLastStatus() != NativeViewport.CAD_AMBIGUOUS_PROFILE);
    }

    // -----------------------------------------------------------------------
    // Debug attribution tokens, read back from this process's own log
    // -----------------------------------------------------------------------

    private String mark() {
        final String marker = "CADMF_MARK_" + System.nanoTime() + "_" + (marks++);
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
                            || trimmed.startsWith("FORGESHAPE_CAD_HUD_TOUCH:")) {
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

    private void fact(String key, Object value) {
        final String line = key + "=" + value;
        facts.add(line);
        Log.i(TAG, "CADMF_DEVICE " + line);
    }

    private void writeFacts() {
        final File file = new File(outDir, "facts-" + System.nanoTime() + ".txt");
        try (PrintWriter out = new PrintWriter(file, "UTF-8")) {
            for (String line : facts) {
                out.println(line);
            }
        } catch (IOException error) {
            Log.w(TAG, "CADMF_DEVICE facts not written: " + error);
        }
    }
}
