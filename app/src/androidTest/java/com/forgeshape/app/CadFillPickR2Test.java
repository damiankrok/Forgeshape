package com.forgeshape.app;

import static com.forgeshape.app.SketchTestSupport.dragSketch;
import static com.forgeshape.app.SketchTestSupport.selectTool;
import static com.forgeshape.app.SketchTestSupport.sketchEntityCount;
import static com.forgeshape.app.SketchTestSupport.sketchState;
import static com.forgeshape.app.SketchTestSupport.tapSketch;
import static com.forgeshape.app.SketchTestSupport.tapWorld;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.os.SystemClock;
import android.util.Log;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
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
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * `CAD-V6-S2-CORRECTION-FILL-PICK-R2` on the device: the fill-bucket selection
 * is a SET (no tap is refused because of another selected cell), cells meeting
 * at a point coexist, a finger that jitters inside the tap slop neither orbits
 * the view nor misses its cell, a still tap on the arrow's SHAFT reaches the
 * cell under it while one on the HEAD stays the arrow's, and an operation
 * refusal keeps the selection.
 *
 * <p>Every tap is a REAL window MotionEvent sequence -- Down, sub-slop Moves,
 * Up -- dispatched to the decor view, so whatever chrome stands over the
 * viewport receives it first, exactly as from a finger. Asserted from native
 * truth (face handles, tool state, the debug camera pose and the debug
 * attribution tokens), never from a picture.
 */
@RunWith(AndroidJUnit4.class)
public final class CadFillPickR2Test {

    private static final String TAG = "ForgeShape";

    private static final String A_ONLY = "a_only";
    private static final String LENS = "lens";
    private static final String B_ONLY = "b_only";
    private static final String C_IN = "c_in";
    private static final String C_OUT = "c_out";
    private static final String LOOP_IN = "loop_in";
    private static final String LOOP_OUT = "loop_out";
    private static final String REST = "rest";
    private static final int CELLS = 8;

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private byte[] baselineProject;
    private File outDir;
    private final List<String> facts = new ArrayList<>();
    private double unit;
    private int marks;

    @Before
    public void setUp() {
        outDir = new File(InstrumentationRegistry.getInstrumentation().getTargetContext()
                .getExternalFilesDir(null), "evidence/cad-v6-s2-fill-pick-r2");
        //noinspection ResultOfMethodCallIgnored
        outDir.mkdirs();
        resetToBaselineConstruction(rule.getScenario());
        baselineProject = NativeViewport.encodeProject();
    }

    @After
    public void tearDown() {
        writeFacts();
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
    // DEV-R2-01: a jittering finger, from both sides of the sketch plane
    // =======================================================================

    @Test
    public void devR2_01_jittered_taps_select_the_same_cell_from_both_sides_without_orbit() {
        drawOwnerSketch();
        finishSketch();
        warmUpFeatureView();
        // Two oblique views on OPPOSITE sides of the XY sketch plane (the eye's
        // z has opposite signs), each in a different world quadrant.
        final float[][] views = {{0.6f, 0.55f}, {(float) Math.PI + 0.6f, 0.55f},
                {-0.9f, -0.5f}, {(float) Math.PI - 0.9f, -0.5f}};
        final String[] cells = {A_ONLY, LENS, B_ONLY, C_IN, LOOP_IN, REST};
        final Map<String, Long> handleOf = new HashMap<>();
        final List<String> problems = new ArrayList<>();
        int jittered = 0;
        for (float[] view : views) {
            setCamera(view[0], view[1], 7.0f);
            for (String cell : cells) {
                final float[] at = tappablePoint(cell, false);
                final String where = cell + "@yaw" + view[0] + "/pitch" + view[1];
                if (at == null) {
                    fact("r2_01." + where, "no tappable point: " + lastBlock);
                    continue;
                }
                final float[] poseBefore = cameraPose();
                assertEquals("nothing chosen before the tap", 0, selected().length);
                final String mark = mark();
                // ~10 px of finger travel in two sub-slop Moves, then Up.
                realGesture(new float[][]{{at[0], at[1]}, {at[0] + 4.0f, at[1] - 3.0f},
                        {at[0] + 8.0f, at[1] + 6.0f}});
                final float[] poseAfter = cameraPose();
                final long[] after = selected();
                final List<String> tokens = tokensSince(mark);
                fact("r2_01." + where, at[0] + "," + at[1] + " selected=" + Arrays.toString(after)
                        + " tokens=" + tokens);
                if (!Arrays.equals(poseBefore, poseAfter)) {
                    problems.add(where + ": the camera moved " + Arrays.toString(poseBefore)
                            + " -> " + Arrays.toString(poseAfter));
                }
                if (after.length != 1) {
                    problems.add(where + ": " + after.length + " cells at " + at[0] + "," + at[1]
                            + " tokens=" + tokens);
                } else {
                    final Long known = handleOf.get(cell);
                    if (known == null) {
                        handleOf.put(cell, after[0]);
                    } else if (known != after[0]) {
                        problems.add(where + ": face " + after[0] + " where another view chose "
                                + known);
                    }
                    jittered++;
                }
                for (long handle : after) {
                    nativeToggle(handle);
                }
                settleLayout();
            }
        }
        assertTrue("every jittered tap lands on its cell without orbit: " + problems,
                problems.isEmpty());
        fact("r2_01.jittered_taps", jittered);
        assertTrue("every cell was tapped from at least two views: " + jittered,
                jittered >= 2 * cells.length);
        assertEquals("six distinct cells", cells.length, handleOf.size());
    }

    // =======================================================================
    // DEV-R2-02: the OWNER mixed sketch, two tap orders, one result
    // =======================================================================

    @Test
    public void devR2_02_two_tap_orders_end_in_the_same_selection_and_preview() {
        drawOwnerSketch();
        finishSketch();
        assertEquals("the OWNER sketch derives eight cells", CELLS, faceHandles().length);
        final String[] first = {A_ONLY, LENS, B_ONLY, C_IN, C_OUT, LOOP_IN, LOOP_OUT, REST};
        final String[] second = {REST, C_OUT, A_ONLY, LOOP_OUT, B_ONLY, LENS, LOOP_IN, C_IN};
        final long[] a = tapAll(first);
        final double[] previewA = candidateMeasure();
        clearSelection();
        final long[] b = tapAll(second);
        final double[] previewB = candidateMeasure();
        assertArrayEquals("the same set of faces", a, b);
        assertEquals("the same preview volume", previewA[0], previewB[0], 1e-9);
        fact("r2_02.selected", Arrays.toString(a));
        fact("r2_02.preview_volume_m3", previewA[0]);
    }

    // =======================================================================
    // DEV-R2-03: two cells meeting only at a point
    // =======================================================================

    @Test
    public void devR2_03_cells_meeting_at_a_point_both_stay_selected_and_commit() {
        drawOwnerSketch();
        finishSketch();
        tapCellStill(A_ONLY);
        tapCellStill(B_ONLY);
        assertEquals("A only and B only both selected", 2, selected().length);
        assertNoTouchAtPointMessage("after A only + B only");
        assertEquals("the candidate is valid", NativeViewport.CAD_OK,
                (int) toolState()[NativeViewport.CAD_EXTRUDE_CANDIDATE_STATUS]);
        final long body = extrudeWithDepth("0.5");
        assertTrue("a New Body", body != NativeViewport.NO_OBJECT);
        final double[] m = measure(body);
        assertEquals("two components touching along an edge", 2.0,
                m[NativeViewport.CAD_MEASURE_COMPONENTS], 0.0);
        fact("r2_03.volume_m3", m[NativeViewport.CAD_MEASURE_VOLUME]);
    }

    // =======================================================================
    // DEV-R2-04: arrow shaft vs head
    // =======================================================================

    @Test
    public void devR2_04_a_still_shaft_tap_selects_a_head_tap_does_not_a_shaft_drag_extrudes() {
        drawOwnerSketch();
        finishSketch();
        // Every cell a shaft pixel may stand over, learned by a real still tap
        // each (and turned off again), so the expected face is a handle and
        // never an inference from where a pixel happens to land.
        final String[] reachable = {A_ONLY, LENS, B_ONLY, C_IN, C_OUT, REST};
        final Map<String, Long> handleOf = new HashMap<>();
        for (String cell : reachable) {
            final long handle = tapCellStill(cell);
            handleOf.put(cell, handle);
            nativeToggle(handle);
        }
        assertEquals("nothing chosen after learning the cells", 0, selected().length);
        nativeToggle(handleOf.get(REST));
        nativeToggle(handleOf.get(LENS));
        final int direction = (int) toolState()[NativeViewport.CAD_EXTRUDE_DIRECTION];
        assertEquals(NativeViewport.CAD_OK, NativeViewport.sketchSetExtrude(1.5, direction));
        settleLayout();
        final float density = onWorkspace(rule.getScenario(),
                (activity, workspace) -> activity.getResources().getDisplayMetrics().density);

        // The search is INVERTED (`CAD-V6-S2-OWNER-CORRECTION-E2E-R1`): it starts
        // from known interiors of bounded cells, projects them, and keeps only a
        // pixel that also lies ON the drawn shaft, clear of the drawn head and of
        // every clickable view. A shaft fraction alone proves nothing about what
        // is under the finger -- attempt 36987358382 tapped empty space there.
        ShaftPick pick = null;
        double[] tool = null;
        float[] head = null;
        float[] pose = null;
        final StringBuilder tried = new StringBuilder();
        search:
        for (float distance : new float[]{7.0f, 9.0f}) {
            for (float pitch : new float[]{0.55f, 0.35f, 0.75f}) {
                for (float yaw : new float[]{0.6f, -0.6f, 2.4f, -2.4f, 1.2f, -1.2f, 0.0f,
                        (float) Math.PI}) {
                    setCamera(yaw, pitch, distance);
                    tool = toolState();
                    final String where = yaw + "/" + pitch + "/" + distance;
                    if (tool[NativeViewport.CAD_EXTRUDE_ON_SCREEN] == 0.0
                            || tool[NativeViewport.CAD_EXTRUDE_HEAD_ON_SCREEN] == 0.0) {
                        tried.append(where).append(" arrow off screen; ");
                        continue;
                    }
                    head = firstViewportPoint(headCandidates(tool));
                    if (head == null) {
                        tried.append(where).append(" head ").append(lastBlock).append("; ");
                        continue;
                    }
                    pick = shaftOverCell(tool, density, handleOf, tried, where);
                    if (pick != null) {
                        pose = new float[]{yaw, pitch, distance};
                        break search;
                    }
                }
            }
        }
        assertNotNull("a pose with a reachable head and a shaft pixel over a known cell: "
                + tried, pick);
        final double depth = tool[NativeViewport.CAD_EXTRUDE_DEPTH];
        final long[] chosen = selected();
        final long expected = handleOf.get(pick.cell);
        // Recorded BEFORE anything is dispatched.
        fact("r2_04.pose_yaw_pitch_distance", Arrays.toString(pose));
        fact("r2_04.depth_m", depth);
        fact("r2_04.shaft_cell", pick.cell + " handle=" + expected);
        fact("r2_04.shaft_uv_units", pick.u + "," + pick.v);
        fact("r2_04.shaft_screen", pick.x + "," + pick.y);
        fact("r2_04.shaft_distance_px", pick.shaftDistance);
        fact("r2_04.shaft_fraction", pick.fraction);
        fact("r2_04.head_clearance_px", pick.headClearance);
        fact("r2_04.shaft_covering_view", pick.covering.isEmpty() ? "none" : pick.covering);
        fact("r2_04.head_screen", head[0] + "," + head[1]);
        fact("r2_04.search", tried.toString());

        // 1. A still tap -- with a finger's sub-slop jitter -- on the SHAFT
        //    toggles exactly the cell under it and leaves the depth.
        final String shaftMark = mark();
        realGesture(new float[][]{{pick.x, pick.y}, {pick.x + 3.0f, pick.y - 2.0f},
                {pick.x + 4.0f, pick.y + 3.0f}});
        final long[] afterShaft = selected();
        final List<String> shaftTokens = tokensSince(shaftMark);
        fact("r2_04.shaft_tap_tokens", shaftTokens);
        assertArrayEquals("a still shaft tap toggles exactly the cell under it: " + shaftTokens,
                new long[]{expected}, symmetricDifference(chosen, afterShaft));
        assertEquals("and leaves the depth", depth, toolState()[NativeViewport.CAD_EXTRUDE_DEPTH],
                0.0);
        assertTrue("resolved: " + shaftTokens,
                shaftTokens.contains("FORGESHAPE_SKETCH_TAP:resolved"));
        assertTrue("never exterior: " + shaftTokens,
                !shaftTokens.contains("FORGESHAPE_SKETCH_TAP:exterior"));
        // Put the selection back exactly, so the arrow stands where it stood.
        nativeToggle(expected);
        assertArrayEquals(chosen, selected());

        // 2. A still tap on the drawn HEAD is the arrow's: no cell, no depth.
        final String headMark = mark();
        realGesture(new float[][]{{head[0], head[1]}});
        final List<String> headTokens = tokensSince(headMark);
        fact("r2_04.head_tap_tokens", headTokens);
        assertArrayEquals("a still head tap toggles no cell", chosen, selected());
        assertEquals("and leaves the depth", depth, toolState()[NativeViewport.CAD_EXTRUDE_DEPTH],
                0.0);
        assertTrue("attributed to the head: " + headTokens,
                headTokens.contains("FORGESHAPE_SKETCH_TAP:arrow_head"));

        // 3. A DRAG from the SAME shaft pixel, in the same pose, takes the arrow.
        final double[] now = toolState();
        final float tx = (float) now[NativeViewport.CAD_EXTRUDE_TIP_X];
        final float ty = (float) now[NativeViewport.CAD_EXTRUDE_TIP_Y];
        final float bx = 2.0f * (float) now[NativeViewport.CAD_EXTRUDE_LABEL_X] - tx;
        final float by = 2.0f * (float) now[NativeViewport.CAD_EXTRUDE_LABEL_Y] - ty;
        final float len = (float) Math.hypot(tx - bx, ty - by);
        final float ux = (tx - bx) / len;
        final float uy = (ty - by) / len;
        final float[][] drag = new float[8][];
        for (int k = 0; k < drag.length; k++) {
            drag[k] = new float[]{pick.x + ux * 12.0f * k, pick.y + uy * 12.0f * k};
        }
        final String dragMark = mark();
        realGesture(drag);
        fact("r2_04.drag_tokens", tokensSince(dragMark));
        assertNotEquals("a shaft drag changes the depth", depth,
                toolState()[NativeViewport.CAD_EXTRUDE_DEPTH], 0.0);
        assertArrayEquals("and toggles no cell", chosen, selected());
        fact("r2_04.depth_after_drag", toolState()[NativeViewport.CAD_EXTRUDE_DEPTH]);
    }

    /** A pixel on the drawn shaft, over a known bounded cell, and what proves it. */
    private static final class ShaftPick {
        String cell;
        double u;
        double v;
        float x;
        float y;
        float shaftDistance;
        float fraction;
        float headClearance;
        String covering = "";
    }

    /** px a pixel may stand off the projected shaft line and still be ON it. */
    private static final float SHAFT_TOLERANCE_PX = 3.0f;
    /** The sketch-unit margin a sampled point keeps from every cell boundary. */
    private static final double CELL_MARGIN_UNITS = 0.3;
    private static final double SAMPLE_STEP_UNITS = 0.05;

    /**
     * The best pixel of this pose that stands on the drawn shaft (within
     * {@link #SHAFT_TOLERANCE_PX}, between 8% and 75% of base to tip), at least
     * 12 dp clear of the head's own tap claim, inside a known cell with
     * {@link #CELL_MARGIN_UNITS} to spare, on the viewport and under no
     * clickable view; null when there is none.
     */
    private ShaftPick shaftOverCell(double[] tool, float density, Map<String, Long> handleOf,
                                    StringBuilder tried, String where) {
        final float tx = (float) tool[NativeViewport.CAD_EXTRUDE_TIP_X];
        final float ty = (float) tool[NativeViewport.CAD_EXTRUDE_TIP_Y];
        final float bx = 2.0f * (float) tool[NativeViewport.CAD_EXTRUDE_LABEL_X] - tx;
        final float by = 2.0f * (float) tool[NativeViewport.CAD_EXTRUDE_LABEL_Y] - ty;
        final float hx = (float) tool[NativeViewport.CAD_EXTRUDE_HEAD_X];
        final float hy = (float) tool[NativeViewport.CAD_EXTRUDE_HEAD_Y];
        // The head cone runs from its ring (tip - axis*h) to its point
        // (tip + axis*h); on screen that is about 2*tip - point .. point.
        final float rx = 2.0f * tx - hx;
        final float ry = 2.0f * ty - hy;
        // The head's still-tap claim: max(10 reference units, the cone's drawn
        // half-width) -- taken generously, the larger of the two bounds.
        final float headHalf = (float) tool[NativeViewport.CAD_EXTRUDE_SCALE]
                * 120.0f * 0.16f;
        final float headClaim = Math.max(10.0f * density, headHalf) + 4.0f * density;
        final float shaftLen = (float) Math.hypot(tx - bx, ty - by);
        if (!(shaftLen > 40.0f * density)) {
            tried.append(where).append(" shaft ").append(shaftLen).append("px too short; ");
            return null;
        }
        final List<ShaftPick> found = new ArrayList<>();
        final float[] at = new float[2];
        for (double u = -4.7; u <= 4.7; u += SAMPLE_STEP_UNITS) {
            for (double v = -8.0; v <= 8.0; v += SAMPLE_STEP_UNITS) {
                final String cell = ownerCellAt(u, v, CELL_MARGIN_UNITS);
                if (cell == null || !handleOf.containsKey(cell)) {
                    continue;
                }
                if (!NativeViewport.sketchScreenPoint(u * unit, v * unit, at)) {
                    continue;
                }
                final float[] seg = segment(at[0], at[1], bx, by, tx, ty);
                if (seg[0] > SHAFT_TOLERANCE_PX || seg[1] < 0.08f || seg[1] > 0.75f) {
                    continue;
                }
                final float clearance = segment(at[0], at[1], rx, ry, hx, hy)[0] - headClaim;
                if (clearance < 12.0f * density) {
                    continue;
                }
                final ShaftPick candidate = new ShaftPick();
                candidate.cell = cell;
                candidate.u = u;
                candidate.v = v;
                candidate.x = at[0];
                candidate.y = at[1];
                candidate.shaftDistance = seg[0];
                candidate.fraction = seg[1];
                candidate.headClearance = clearance;
                found.add(candidate);
            }
        }
        // Nearest the drawn line first; then the most head clearance.
        found.sort((a, b) -> a.shaftDistance != b.shaftDistance
                ? Float.compare(a.shaftDistance, b.shaftDistance)
                : Float.compare(b.headClearance, a.headClearance));
        final StringBuilder blocked = new StringBuilder();
        for (int i = 0; i < found.size() && i < 60; i++) {
            final ShaftPick candidate = found.get(i);
            if (firstViewportPoint(new float[][]{{candidate.x, candidate.y}}) != null) {
                candidate.covering = blocked.toString();
                return candidate;
            }
            blocked.append(candidate.cell).append(lastBlock).append(' ');
        }
        tried.append(where).append(' ').append(found.size()).append(" shaft-over-cell pixels")
                .append(blocked.length() > 0 ? " all covered: " + blocked : "").append("; ");
        return null;
    }

    /** {distance, clamped parameter} from (x, y) to the segment a..b, in px. */
    private static float[] segment(float x, float y, float ax, float ay, float bx, float by) {
        final float dx = bx - ax;
        final float dy = by - ay;
        final float len2 = dx * dx + dy * dy;
        float t = len2 > 0.0f ? ((x - ax) * dx + (y - ay) * dy) / len2 : 0.0f;
        t = Math.max(0.0f, Math.min(1.0f, t));
        return new float[]{(float) Math.hypot(x - (ax + dx * t), y - (ay + dy * t)), t};
    }

    /**
     * Which OWNER-sketch cell holds (u, v) -- in sketch units -- with
     * `margin` to spare from every boundary, or null when the point is near a
     * boundary, outside every bounded cell, or anywhere near the spline loop
     * (whose interpolated curve this test does not model).
     */
    private static String ownerCellAt(double u, double v, double margin) {
        if (u > -3.9 && u < -0.1 && v > -9.9 && v < -4.5) {
            return null;  // the spline loop and the line closing it
        }
        final double inRect = Math.min(4.0 - Math.abs(u), 6.0 - Math.abs(v));
        final double a = 2.0 - Math.hypot(u + 1.0, v - 1.0);
        final double b = 2.0 - Math.hypot(u - 1.0, v - 1.0);
        final double c = 2.0 - Math.hypot(u, v - 6.0);
        if (Math.abs(inRect) < margin || Math.abs(a) < margin || Math.abs(b) < margin
                || Math.abs(c) < margin) {
            return null;
        }
        if (inRect < 0.0) {
            return c > 0.0 ? C_OUT : null;
        }
        if (a > 0.0 && b > 0.0) return LENS;
        if (a > 0.0) return A_ONLY;
        if (b > 0.0) return B_ONLY;
        if (c > 0.0) return C_IN;
        return REST;
    }

    // =======================================================================
    // DEV-R2-05: an operation refusal keeps the selection
    // =======================================================================

    @Test
    public void devR2_05_an_add_refused_as_disjoint_keeps_every_selected_cell() {
        baseBodyOnXz();
        assertTrue(NativeViewport.debugSetCameraPose(0.7f, 0.9f, 8.0f));
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(NativeViewport.supportChooserBegin(true));
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        tapWorld(rule.getScenario(), 0.0, 1.0, 0.0);
        tapWorld(rule.getScenario(), 0.0, 1.0, 0.0);
        assertEquals("a sketch on the base's top cap", NativeViewport.SKETCH_EDITING,
                sketchState());
        final double grid = NativeViewport.sketchGridStep();
        unit = grid * Math.max(1.0, Math.rint(0.2 / grid));
        final double s = unit;
        // A square on the cap crossed by a circle (so the sketch is on faces),
        // and a far square beyond the cap's edge.
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), -2 * s, -2 * s, 2 * s, 2 * s);
        dragSketch(rule.getScenario(), 8 * s, -1 * s, 10 * s, 1 * s);
        selectTool(rule.getScenario(), R.id.tool_rail_circle);
        dragSketch(rule.getScenario(), 2 * s, 0, 3 * s, 0);
        finishSketch();
        assertEquals("a crossing sketch is on faces", 1, NativeViewport.sketchSelectionKind());
        assertEquals(NativeViewport.CAD_OK,
                NativeViewport.sketchSetOperation(NativeViewport.OPERATION_ADD));
        settleLayout();
        final long inside = turnOnAt(-1 * s, 0);
        final long far = turnOnAt(9 * s, 0);
        assertTrue(inside != far);
        final long[] held = selected();
        assertEquals("both cells selected", 2, held.length);
        assertEquals("the Add is refused for its operation", NativeViewport.CAD_ADD_DISJOINT,
                (int) toolState()[NativeViewport.CAD_EXTRUDE_CANDIDATE_STATUS]);
        final String line = statusLine();
        assertTrue("the status line names the operation problem: " + line,
                line.contains(string(R.string.status_cad_add_disjoint)));
        assertArrayEquals("the selection stands", held, selected());
        // Removing the cell that made it disjoint restores the preview.
        nativeToggle(far);
        settleLayout();
        assertEquals(NativeViewport.CAD_OK,
                (int) toolState()[NativeViewport.CAD_EXTRUDE_CANDIDATE_STATUS]);
        fact("r2_05.status_line", line);
    }

    // =======================================================================
    // DEV-R2-06: debug attribution tokens
    // =======================================================================

    @Test
    public void devR2_06_tap_and_hud_attribution_tokens_are_logged() {
        drawOwnerSketch();
        finishSketch();
        final String resolvedMark = mark();
        tapCellStill(LENS);
        assertTrue("a resolved face tap: " + tokensSince(resolvedMark),
                tokensSince(resolvedMark).contains("FORGESHAPE_SKETCH_TAP:resolved"));

        setCamera(0.6f, 0.55f, 7.0f);
        final double[] tool = toolState();
        final float[] head = firstViewportPoint(new float[][]{
                {(float) tool[NativeViewport.CAD_EXTRUDE_HEAD_X],
                        (float) tool[NativeViewport.CAD_EXTRUDE_HEAD_Y]},
                {(float) tool[NativeViewport.CAD_EXTRUDE_TIP_X],
                        (float) tool[NativeViewport.CAD_EXTRUDE_TIP_Y]}});
        assertNotNull(head);
        final String headMark = mark();
        realGesture(new float[][]{{head[0], head[1]}});
        assertTrue("an arrow-head tap: " + tokensSince(headMark),
                tokensSince(headMark).contains("FORGESHAPE_SKETCH_TAP:arrow_head"));

        // A Down the HUD consumes: the value label.
        final float[] value = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View label = workspace.cadExtrudeCanvas().findViewById(R.id.cad_extrude_depth_value);
            if (label == null || !label.isShown()) {
                return null;
            }
            final int[] at = new int[2];
            label.getLocationInWindow(at);
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final int[] vp = new int[2];
            viewport.getLocationInWindow(vp);
            return new float[]{at[0] - vp[0] + label.getWidth() / 2f,
                    at[1] - vp[1] + label.getHeight() / 2f};
        });
        assertNotNull("the value label is shown", value);
        final String hudMark = mark();
        realGesture(new float[][]{{value[0], value[1]}});
        final List<String> hud = tokensSince(hudMark);
        assertTrue("a HUD-consumed Down: " + hud, hud.contains("FORGESHAPE_CAD_HUD_TOUCH:value"));
        fact("r2_06.tokens", tokensSince(resolvedMark));
    }

    // -----------------------------------------------------------------------
    // The OWNER sketch (as CadPlanarFaceOwnerCorrectionTest draws it)
    // -----------------------------------------------------------------------

    private void drawOwnerSketch() {
        beginSketch();
        final double grid = NativeViewport.sketchGridStep();
        assertTrue("the grid step is fine enough to draw on: " + grid, grid > 0.0 && grid <= 0.25);
        unit = grid * Math.max(1.0, Math.rint(0.2 / grid));
        final double s = unit;
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), -4 * s, -6 * s, 4 * s, 6 * s);
        selectTool(rule.getScenario(), R.id.tool_rail_circle);
        dragSketch(rule.getScenario(), -1 * s, 1 * s, 1 * s, 1 * s);
        dragSketch(rule.getScenario(), 1 * s, 1 * s, 3 * s, 1 * s);
        dragSketch(rule.getScenario(), 0, 6 * s, 2 * s, 6 * s);
        selectTool(rule.getScenario(), R.id.tool_rail_spline);
        tapSketch(rule.getScenario(), -3 * s, -5 * s);
        tapSketch(rule.getScenario(), -2 * s, -9 * s);
        tapSketch(rule.getScenario(), -1 * s, -5 * s);
        tapSketch(rule.getScenario(), -1 * s, -5 * s);
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        dragSketch(rule.getScenario(), -1 * s, -5 * s, -3 * s, -5 * s);
        assertEquals("rectangle, three circles, the spline and its line", 6, sketchEntityCount());
    }

    private double[][] candidates(String cell) {
        switch (cell) {
            case A_ONLY:
                return new double[][]{{-2, 1}, {-2, 1.5}, {-2, 0.5}, {-2.5, 1}};
            case LENS:
                return new double[][]{{0, 1}, {0, 1.5}, {0, 0.5}, {0, 1.2}};
            case B_ONLY:
                return new double[][]{{2, 1}, {2, 1.5}, {2, 0.5}, {2.5, 1}};
            case C_IN:
                return new double[][]{{0, 5}, {0.5, 5}, {-0.5, 5}, {0, 4.5}};
            case C_OUT:
                return new double[][]{{0, 7}, {0.5, 7}, {-0.5, 7}, {0, 7.5}};
            case LOOP_IN:
                return new double[][]{{-2, -5.5}, {-2.3, -5.5}, {-1.7, -5.5}, {-2, -5.7}};
            case LOOP_OUT:
                return new double[][]{{-2, -7.5}, {-2, -8}, {-2, -7}, {-2.2, -7.3}};
            case REST:
                return new double[][]{{3, -3}, {-3, 4}, {3, 4}, {2, -5}};
            default:
                throw new IllegalArgumentException(cell);
        }
    }

    // -----------------------------------------------------------------------
    // Taps
    // -----------------------------------------------------------------------

    /** Selects the Ready feature view once, so later camera poses are the test's own. */
    private void warmUpFeatureView() {
        tapCellStill(REST);
        for (long handle : selected()) {
            nativeToggle(handle);
        }
        settleLayout();
        assertEquals(0, selected().length);
    }

    /** Points ON the drawn head: its point, the cone between, its middle (the tip). */
    private static float[][] headCandidates(double[] tool) {
        final float hx = (float) tool[NativeViewport.CAD_EXTRUDE_HEAD_X];
        final float hy = (float) tool[NativeViewport.CAD_EXTRUDE_HEAD_Y];
        final float tx = (float) tool[NativeViewport.CAD_EXTRUDE_TIP_X];
        final float ty = (float) tool[NativeViewport.CAD_EXTRUDE_TIP_Y];
        final float[] fractions = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
        final float[][] out = new float[fractions.length][];
        for (int i = 0; i < fractions.length; i++) {
            out[i] = new float[]{hx + (tx - hx) * fractions[i], hy + (ty - hy) * fractions[i]};
        }
        return out;
    }

    /** The first candidate point of a cell a finger can reach, or null. */
    private float[] tappablePoint(String cell, boolean avoidArrow) {
        for (double[] p : candidates(cell)) {
            final float[] at = new float[2];
            if (!NativeViewport.sketchScreenPoint(p[0] * unit, p[1] * unit, at)) {
                continue;
            }
            if (avoidArrow && shaftDistance(at[0], at[1]) < 48.0f) {
                continue;
            }
            if (firstViewportPoint(new float[][]{at}) != null) {
                return at;
            }
        }
        return null;
    }

    /** One still tap on a cell; asserts it turned exactly that cell on. */
    private long tapCellStill(String cell) {
        final long[] before = selected();
        final float[] at = tappablePoint(cell, true);
        assertNotNull("a tappable point of " + cell, at);
        realGesture(new float[][]{{at[0], at[1]}});
        final long[] after = selected();
        assertEquals(cell + " joins the selection (status " + NativeViewport.sketchLastStatus() + ")",
                before.length + 1, after.length);
        final long[] added = symmetricDifference(before, after);
        assertEquals(1, added.length);
        fact("tap." + cell, at[0] + "," + at[1] + " handle=" + added[0]);
        return added[0];
    }

    private long[] tapAll(String[] order) {
        for (int i = 0; i < order.length; i++) {
            tapCellStill(order[i]);
            assertNoTouchAtPointMessage("after " + order[i]);
            assertEquals("the union candidate is valid after " + order[i], NativeViewport.CAD_OK,
                    (int) toolState()[NativeViewport.CAD_EXTRUDE_CANDIDATE_STATUS]);
        }
        return selected();
    }

    /** A still tap at a sketch point; returns the handle it turned on. */
    private long turnOnAt(double u, double v) {
        final float[] at = new float[2];
        assertTrue(NativeViewport.sketchScreenPoint(u, v, at));
        final float[] reach = firstViewportPoint(new float[][]{at});
        assertNotNull("(" + u + ", " + v + ") reaches the viewport", reach);
        final long[] before = selected();
        realGesture(new float[][]{{reach[0], reach[1]}});
        final long[] added = symmetricDifference(before, selected());
        assertEquals("the tap toggled exactly one cell", 1, added.length);
        return added[0];
    }

    /**
     * The first point that is on the viewport and that a real finger would
     * deliver to the viewport itself (no clickable chrome above it), or null.
     */
    /** Why the last {@link #firstViewportPoint} candidate was refused. Evidence only. */
    private String lastBlock = "";

    private float[] firstViewportPoint(float[][] points) {
        final StringBuilder why = new StringBuilder();
        for (float[] at : points) {
            final String block = onWorkspace(rule.getScenario(), (activity, workspace) -> {
                final float density = activity.getResources().getDisplayMetrics().density;
                final View viewport = workspace.findViewById(R.id.viewport_surface);
                if (at[0] < 8 * density || at[1] < 8 * density
                        || at[0] > viewport.getWidth() - 8 * density
                        || at[1] > viewport.getHeight() - 8 * density) {
                    return "off the viewport " + viewport.getWidth() + "x" + viewport.getHeight();
                }
                final View root = activity.getWindow().getDecorView();
                final int[] vp = new int[2];
                viewport.getLocationInWindow(vp);
                final View owner = clickableAt(root, at[0] + vp[0], at[1] + vp[1]);
                if (owner == null || owner == viewport) {
                    return null;
                }
                String name;
                try {
                    name = owner.getId() == View.NO_ID ? "no-id"
                            : activity.getResources().getResourceEntryName(owner.getId());
                } catch (RuntimeException unnamed) {
                    name = "#" + owner.getId();
                }
                return "under " + owner.getClass().getSimpleName() + ":" + name;
            });
            if (block == null) {
                lastBlock = "";
                return at;
            }
            why.append('[').append(at[0]).append(',').append(at[1]).append(' ').append(block)
                    .append(']');
        }
        lastBlock = why.toString();
        return null;
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
        return view.isClickable() || view.getId() == R.id.viewport_surface ? view : null;
    }

    /** px from the drawn shaft (base to the arrow point); infinite with no arrow. */
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
     * One finger through the WINDOW: Down at the first viewport point, a Move
     * to each later one, Up at the last. Whatever stands over the viewport
     * receives it first, as from a real touch screen.
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
    // Session plumbing
    // -----------------------------------------------------------------------

    private void beginSketch() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openAddPrimitive(workspace);
            workspace.addPrimitivePalette().findViewById(R.id.sketch_plane_by_name).performClick();
            workspace.addPrimitivePalette().findViewById(R.id.sketch_plane_xy).performClick();
            return null;
        });
        settleLayout();
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
    }

    private void beginSketchOn(int planeId) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openAddPrimitive(workspace);
            workspace.addPrimitivePalette().findViewById(R.id.sketch_plane_by_name).performClick();
            workspace.addPrimitivePalette().findViewById(planeId).performClick();
            return null;
        });
        settleLayout();
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
    }

    private long baseBodyOnXz() {
        beginSketchOn(R.id.sketch_plane_xz);
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), -1.0, -1.0, 1.0, 1.0);
        finishSketch();
        final long base = extrudeWithDepth("1");
        assertTrue("a base body", base != NativeViewport.NO_OBJECT);
        return base;
    }

    private void finishSketch() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.finish_sketch).performClick();
            return null;
        });
        settleLayout();
        assertEquals("Finish succeeds: " + NativeViewport.sketchLastStatus(),
                NativeViewport.SKETCH_READY, sketchState());
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

    /**
     * Toggles a face through the native call AND tells the workspace, as every
     * product path does, so no HUD view keeps standing where the previous
     * selection put it and takes the next Down.
     */
    private void nativeToggle(long handle) {
        final int status = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int why = NativeViewport.sketchToggleRegion(handle);
            workspace.onNativeStateChanged();
            return why;
        });
        assertEquals(NativeViewport.CAD_OK, status);
        settleLayout();
    }

    private void clearSelection() {
        for (long handle : selected()) {
            nativeToggle(handle);
        }
        settleLayout();
        assertEquals(0, selected().length);
    }

    private static long[] faceHandles() {
        final int count = NativeViewport.sketchProfiles(null);
        final long[] handles = new long[Math.max(count, 0)];
        assertEquals(count, NativeViewport.sketchProfiles(handles));
        return handles;
    }

    /** The selected face handles, ascending. */
    private static long[] selected() {
        final List<Long> out = new ArrayList<>();
        for (long handle : faceHandles()) {
            final double[] info = new double[NativeViewport.SKETCH_REGION_INFO_SIZE];
            assertTrue(NativeViewport.sketchProfileInfo(handle, info));
            if (info[NativeViewport.SKETCH_REGION_SELECTED] != 0.0) {
                out.add(handle);
            }
        }
        final long[] handles = new long[out.size()];
        for (int i = 0; i < handles.length; i++) {
            handles[i] = out.get(i);
        }
        Arrays.sort(handles);
        return handles;
    }

    private static long[] symmetricDifference(long[] a, long[] b) {
        final List<Long> out = new ArrayList<>();
        for (long x : a) {
            if (Arrays.binarySearch(b, x) < 0) out.add(x);
        }
        for (long x : b) {
            if (Arrays.binarySearch(a, x) < 0) out.add(x);
        }
        final long[] result = new long[out.size()];
        for (int i = 0; i < result.length; i++) {
            result[i] = out.get(i);
        }
        return result;
    }

    private static double[] toolState() {
        final double[] tool = new double[NativeViewport.CAD_EXTRUDE_SIZE];
        NativeViewport.cadExtrudeToolState(tool);
        return tool;
    }

    private static double[] candidateMeasure() {
        final double[] out = new double[NativeViewport.CANDIDATE_MEASURE_SIZE];
        assertTrue("the candidate measures", NativeViewport.sketchCandidateMeasure(out));
        assertEquals("the candidate is valid", NativeViewport.CAD_OK,
                (int) out[NativeViewport.CANDIDATE_MEASURE_STATUS]);
        return new double[]{out[NativeViewport.CANDIDATE_MEASURE_VOLUME],
                out[NativeViewport.CANDIDATE_MEASURE_COMPONENTS]};
    }

    private static double[] measure(long body) {
        final double[] out = new double[NativeViewport.CAD_MEASURE_SIZE];
        assertTrue("the body measures as a CAD solid", NativeViewport.cadBodyMeasure(body, out));
        return out;
    }

    private String statusLine() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View message = workspace.findViewById(R.id.status_message);
            return message instanceof TextView ? ((TextView) message).getText().toString() : "";
        });
    }

    private String string(int id) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> activity.getString(id));
    }

    private void assertNoTouchAtPointMessage(String where) {
        assertNotEquals(where + ": no point-touch selection refusal",
                NativeViewport.CAD_PLANAR_FACES_TOUCH_AT_POINT, NativeViewport.sketchLastStatus());
        final String line = statusLine();
        assertTrue(where + ": the status line: " + line,
                !line.contains(string(R.string.status_cad_areas_touch_at_point)));
    }

    // -----------------------------------------------------------------------
    // Debug attribution tokens, read back from this process's own log
    // -----------------------------------------------------------------------

    private String mark() {
        final String marker = "CADR2_MARK_" + System.nanoTime() + "_" + (marks++);
        Log.i(TAG, marker);
        return marker;
    }

    /** The R2 token lines logged after `marker`, in order (`FORGESHAPE_…:<reason>`). */
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
        Log.i(TAG, "CADR2_DEVICE " + line);
    }

    private void writeFacts() {
        final File file = new File(outDir, "facts-" + System.nanoTime() + ".txt");
        try (PrintWriter out = new PrintWriter(file, "UTF-8")) {
            for (String line : facts) {
                out.println(line);
            }
        } catch (IOException error) {
            Log.w(TAG, "CADR2_DEVICE facts not written: " + error);
        }
    }
}
