package com.forgeshape.app;

import static com.forgeshape.app.SketchTestSupport.dragSketch;
import static com.forgeshape.app.SketchTestSupport.selectTool;
import static com.forgeshape.app.SketchTestSupport.sketchEntityCount;
import static com.forgeshape.app.SketchTestSupport.sketchState;
import static com.forgeshape.app.SketchTestSupport.tapSketch;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.graphics.Bitmap;
import android.os.SystemClock;
import android.util.Log;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.widget.EditText;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;

/**
 * `CAD-V6-S2-CORRECTION-FILL-HUD-R1` on the device, the way the OWNER uses it:
 * one sketch of a rectangle, two overlapping circles, a circle across the
 * rectangle's side and a spline-and-line loop across its bottom side, every
 * bounded cell picked by a REAL tap on the viewport — dispatched through the
 * window, so the extrude HUD standing over the drawing gets every touch it
 * would get from a finger — and never through the precision surface's rows.
 *
 * <p>J1 taps all eight cells one by one with the preview, the arrow and the
 * action panel already on screen; J2 is the fill toggle A, B, A; J3 commits a
 * union of adjacent cells; J4 extrudes a spline-bounded cell; J5 orbits the
 * camera and records the action panel staying attached at the arrow's tip.
 * Asserted from native truth, never from a picture; the pictures are evidence.
 */
@RunWith(AndroidJUnit4.class)
public final class CadPlanarFaceOwnerCorrectionTest {

    private static final String TAG = "ForgeShape";

    /** The OWNER stress sketch derives exactly eight bounded cells (FILL-06). */
    private static final int CELLS = 8;
    /** Cell names, in the order J1 taps them (each adjacent to or apart from the rest). */
    private static final String A_ONLY = "a_only";
    private static final String LENS = "lens";
    private static final String B_ONLY = "b_only";
    private static final String C_IN = "c_in";
    private static final String C_OUT = "c_out";
    private static final String LOOP_IN = "loop_in";
    private static final String LOOP_OUT = "loop_out";
    private static final String REST = "rest";

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private byte[] baselineProject;
    private File outDir;
    private final List<String> facts = new ArrayList<>();
    /** The sketch unit: a multiple of the grid step near 0.2 m, so every tap is exact. */
    private double unit;

    @Before
    public void setUp() {
        outDir = new File(InstrumentationRegistry.getInstrumentation().getTargetContext()
                .getExternalFilesDir(null), "evidence/cad-v6-s2-correction");
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
    // J1: every bounded cell of the OWNER's sketch, by a real tap each
    // =======================================================================

    @Test
    public void j1_every_cell_of_the_mixed_sketch_is_tapped_into_the_selection() {
        drawOwnerSketch();
        finishSketch();
        assertEquals("a mixed sketch with a spline is PlanarFaces", 1,
                NativeViewport.sketchSelectionKind());
        assertEquals("exactly the eight bounded cells", CELLS, faceHandles().length);
        capture("j1_00_finished");
        final String[] order = {A_ONLY, LENS, B_ONLY, C_IN, C_OUT, LOOP_IN, LOOP_OUT, REST};
        int inCorridor = 0;
        for (int i = 0; i < order.length; i++) {
            if (i > 0) {
                assertTrue("the extrude preview is up before tap " + (i + 1),
                        toolState()[NativeViewport.CAD_EXTRUDE_ACTIVE] != 0.0);
            }
            final Tap tap = tapCell(order[i]);
            inCorridor += tap.inArrowCorridor ? 1 : 0;
            assertEquals("tap " + (i + 1) + " (" + order[i] + ") adds exactly its cell",
                    i + 1, selectedCount());
            assertNoLegacyOverlap("after " + order[i]);
            assertEquals("the union is a valid candidate after " + order[i], NativeViewport.CAD_OK,
                    (int) toolState()[NativeViewport.CAD_EXTRUDE_CANDIDATE_STATUS]);
            if (i == 2) {
                capture("j1_03_three_cells");
            }
        }
        fact("j1.taps_inside_arrow_corridor", inCorridor);
        capture("j1_08_all_cells");
    }

    // =======================================================================
    // J2: fill toggle A, B, A leaves B
    // =======================================================================

    @Test
    public void j2_fill_toggle_a_b_a_leaves_only_b() {
        drawOwnerSketch();
        finishSketch();
        tapCell(LENS);
        final long b = tapCell(B_ONLY).turnedOn;
        assertEquals(2, selectedCount());
        tapCell(LENS);
        assertEquals("A off again: one cell", 1, selectedCount());
        assertEquals("and it is B", 1.0, info(b)[NativeViewport.SKETCH_REGION_SELECTED], 0.0);
        assertNoLegacyOverlap("after A, B, A");
    }

    // =======================================================================
    // J3: adjacent cells commit as ONE solid with no internal wall
    // =======================================================================

    @Test
    public void j3_adjacent_cells_union_into_one_solid() {
        drawOwnerSketch();
        finishSketch();
        double area = 0.0;
        for (String cell : new String[]{A_ONLY, LENS, B_ONLY}) {
            area += info(tapCell(cell).turnedOn)[NativeViewport.SKETCH_REGION_AREA];
        }
        final int bodies = NativeViewport.sceneBodyCount();
        final long body = extrudeWithDepth("0.5");
        assertTrue("a New Body", body != NativeViewport.NO_OBJECT);
        assertEquals(bodies + 1, NativeViewport.sceneBodyCount());
        assertEquals("PlanarFaces stored", 1, NativeViewport.cadFeatureSelectionKind(body, 0));
        final double[] m = measure(body);
        assertEquals("one shell: the shared arcs are no wall", 1.0,
                m[NativeViewport.CAD_MEASURE_COMPONENTS], 0.0);
        assertEquals("the two overlapping disks, 0.5 deep", area * 0.5,
                m[NativeViewport.CAD_MEASURE_VOLUME], area * 0.5 * 0.02);
        fact("j3.union_volume_m3", m[NativeViewport.CAD_MEASURE_VOLUME]);
    }

    // =======================================================================
    // J4: a spline-bounded cell becomes a New Body
    // =======================================================================

    @Test
    public void j4_a_spline_bounded_cell_extrudes_as_a_new_body() {
        drawOwnerSketch();
        finishSketch();
        final long cell = tapCell(LOOP_OUT).turnedOn;
        final double area = info(cell)[NativeViewport.SKETCH_REGION_AREA];
        assertTrue("the cell below the rectangle has an area", area > 0.0);
        final long body = extrudeWithDepth("0.5");
        assertTrue("a New Body", body != NativeViewport.NO_OBJECT);
        assertEquals(1, NativeViewport.cadFeatureSelectionKind(body, 0));
        final double[] m = measure(body);
        assertEquals("one shell", 1.0, m[NativeViewport.CAD_MEASURE_COMPONENTS], 0.0);
        assertEquals("the spline cell, 0.5 deep", area * 0.5,
                m[NativeViewport.CAD_MEASURE_VOLUME], area * 0.5 * 0.02);
        fact("j4.spline_cell_volume_m3", m[NativeViewport.CAD_MEASURE_VOLUME]);
    }

    // =======================================================================
    // J5: the action panel stays attached through an orbit
    // =======================================================================

    @Test
    public void j5_the_action_panel_moves_continuously_through_an_orbit() {
        drawOwnerSketch();
        finishSketch();
        tapCell(LENS);
        float previousOffsetX = Float.NaN;
        float previousOffsetY = Float.NaN;
        float previousRotation = Float.NaN;
        float largestJump = 0.0f;
        float largestTurn = 0.0f;
        int shown = 0;
        for (int step = 0; step <= 24; step++) {
            final float yaw = 0.35f + 0.025f * step;
            final float pitch = 0.75f - 0.01f * step;
            setCamera(yaw, pitch, 7.0f);
            final Object[] frame = onWorkspace(rule.getScenario(), (activity, workspace) -> {
                final float density = activity.getResources().getDisplayMetrics().density;
                final CadExtrudeCanvasView canvas = workspace.cadExtrudeCanvas();
                final View viewport = workspace.findViewById(R.id.viewport_surface);
                final double[] tool = toolState();
                final CadHudPresentation.PanelLayout panel = canvas.lastPanelLayout();
                final String why = panel.visible ? CadLeaderHudChecks.panelAtArrow(tool, canvas,
                        viewport, density, CadHudPresentation.panelIconCount(
                                (int) tool[NativeViewport.CAD_EXTRUDE_EXTENT])) : null;
                return new Object[]{panel, why, density, tool};
            });
            final CadHudPresentation.PanelLayout panel = (CadHudPresentation.PanelLayout) frame[0];
            assertNull("step " + step + ": " + frame[1], frame[1]);
            final float density = (Float) frame[2];
            if (!panel.visible) {
                fact("j5.step" + step, "hidden");
                previousOffsetX = Float.NaN;
                continue;
            }
            shown++;
            // The panel's offset from the arrow's point: it may follow the
            // point anywhere, but between two small orbit steps it must not
            // jump to another place around it.
            final float offsetX = panel.centreX - panel.anchorX;
            final float offsetY = panel.centreY - panel.anchorY;
            if (!Float.isNaN(previousOffsetX)) {
                largestJump = Math.max(largestJump, (float) Math.hypot(
                        offsetX - previousOffsetX, offsetY - previousOffsetY) / density);
                largestTurn = Math.max(largestTurn, Math.abs(panel.rotation - previousRotation));
            }
            previousOffsetX = offsetX;
            previousOffsetY = offsetY;
            previousRotation = panel.rotation;
            fact("j5.step" + step, "centre=" + panel.centreX + "," + panel.centreY
                    + " anchor=" + panel.anchorX + "," + panel.anchorY + " rotation="
                    + panel.rotation + " slide_dp=" + panel.slide / density + " scale="
                    + panel.scale);
            if (step == 0 || step == 12 || step == 24) {
                capture("j5_orbit_step_" + step);
            }
        }
        fact("j5.shown_frames", shown);
        fact("j5.largest_offset_jump_dp", largestJump);
        fact("j5.largest_turn_deg", largestTurn);
        assertTrue("the panel stands for most of the orbit: " + shown, shown >= 13);
        assertTrue("no side jump between small orbit steps: " + largestJump + " dp",
                largestJump < 0.5f * CadHudPresentation.panelReferenceWidthDp(3));
        assertTrue("no violent turn: " + largestTurn + " deg", largestTurn < 6.0f);
        // Close, normal and far, for the record.
        for (float distance : new float[]{3.5f, 7.0f, 14.0f}) {
            setCamera(0.65f, 0.6f, distance);
            capture("j5_distance_" + (int) (distance * 10));
        }
    }

    // -----------------------------------------------------------------------
    // The sketch
    // -----------------------------------------------------------------------

    /**
     * The OWNER's stress case in sketch units {@code s} — the grid step, or a
     * multiple of it near 0.2 m — with every drawn point a WHOLE number of
     * units, so every point lands exactly on the grid it snaps to:
     * an 8 x 12 rectangle centred at the origin; circles A at (-1, 1) and B at
     * (1, 1), both of radius 2, overlapping inside it; circle C at (0, 6),
     * radius 2, across its top side; and a spline from (-3, -5) down through
     * (-2, -9) to (-1, -5), closed by a line, across its bottom side. The
     * cells FILL-06 pins: A only, the lens, B only, C inside and outside, the
     * loop above and below the bottom side, and the rest of the rectangle.
     */
    private void drawOwnerSketch() {
        beginSketch();
        final double grid = NativeViewport.sketchGridStep();
        assertTrue("the grid step is fine enough to draw on: " + grid, grid > 0.0 && grid <= 0.25);
        unit = grid * Math.max(1.0, Math.rint(0.2 / grid));
        fact("sketch.grid_step", grid);
        fact("sketch.unit", unit);
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

    /** Points known to lie in each cell, in sketch units, tried in order. */
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
    // Real taps
    // -----------------------------------------------------------------------

    private static final class Tap {
        long turnedOn = NativeViewport.NO_OBJECT;
        boolean inArrowCorridor;
    }

    /**
     * One REAL tap on a cell: the first candidate point that projects onto the
     * viewport, is not under the action panel (which may own the touches on its
     * own box) and is not on the drawn arrow itself, dispatched through the
     * window. Returns the handle the tap toggled ON, if any.
     */
    private Tap tapCell(String cell) {
        final long[] before = faceHandles();
        final double[] selectedBefore = new double[before.length];
        for (int i = 0; i < before.length; i++) {
            selectedBefore[i] = info(before[i])[NativeViewport.SKETCH_REGION_SELECTED];
        }
        final Tap tap = new Tap();
        float[] chosen = null;
        for (double[] p : candidates(cell)) {
            final float[] at = new float[2];
            if (!NativeViewport.sketchScreenPoint(p[0] * unit, p[1] * unit, at)) {
                continue;
            }
            final String why = onWorkspace(rule.getScenario(), (activity, workspace) -> {
                final float density = activity.getResources().getDisplayMetrics().density;
                final View viewport = workspace.findViewById(R.id.viewport_surface);
                if (at[0] < 8 * density || at[1] < 8 * density
                        || at[0] > viewport.getWidth() - 8 * density
                        || at[1] > viewport.getHeight() - 8 * density) {
                    return "off the viewport";
                }
                // A real finger there would press whatever chrome stands over
                // the viewport, so the point must reach the viewport itself.
                final View root = activity.getWindow().getDecorView();
                final int[] vp = new int[2];
                viewport.getLocationInWindow(vp);
                final View owner = clickableAt(root, at[0] + vp[0], at[1] + vp[1]);
                if (owner != null && owner != viewport
                        && owner.getId() != R.id.cad_extrude_panel) {
                    return "under chrome " + owner.getClass().getSimpleName() + "#" + owner.getId();
                }
                final CadHudPresentation.PanelLayout panel =
                        workspace.cadExtrudeCanvas().lastPanelLayout();
                if (panel != null && toolState()[NativeViewport.CAD_EXTRUDE_ACTIVE] != 0.0
                        && CadHudPresentation.panelOwnsTouch(panel, at[0], at[1])) {
                    return "under the action panel";
                }
                final float shaft = shaftDistance(at[0], at[1]);
                if (shaft < 16.0f * density) {
                    return "on the drawn arrow";
                }
                tap.inArrowCorridor = shaft <= 24.0f * density;
                return null;
            });
            if (why == null) {
                chosen = at;
                break;
            }
            fact("tap." + cell + ".skipped", why);
        }
        assertTrue("a tappable point of " + cell, chosen != null);
        realTap(chosen[0], chosen[1]);
        final long[] after = faceHandles();
        assertEquals("the cells do not change under a tap", before.length, after.length);
        for (int i = 0; i < after.length; i++) {
            if (selectedBefore[i] == 0.0 && info(after[i])[NativeViewport.SKETCH_REGION_SELECTED] != 0.0) {
                tap.turnedOn = after[i];
            }
        }
        fact("tap." + cell, chosen[0] + "," + chosen[1] + " corridor=" + tap.inArrowCorridor
                + " status=" + NativeViewport.sketchLastStatus());
        return tap;
    }

    /**
     * The topmost visible, clickable view under a WINDOW point, or null:
     * children are searched last-drawn first, as touch dispatch does.
     */
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

    /**
     * The tap point's distance from the drawn shaft, px: the segment from the
     * base (twice the shaft's middle minus its tip, both projected below JNI)
     * to the arrow's point. Infinite while no arrow stands.
     */
    private static float shaftDistance(float x, float y) {
        final double[] tool = toolState();
        if (tool[NativeViewport.CAD_EXTRUDE_ACTIVE] == 0.0
                || tool[NativeViewport.CAD_EXTRUDE_ON_SCREEN] == 0.0) {
            return Float.POSITIVE_INFINITY;
        }
        final float lx = (float) tool[NativeViewport.CAD_EXTRUDE_LABEL_X];
        final float ly = (float) tool[NativeViewport.CAD_EXTRUDE_LABEL_Y];
        final float tx = (float) tool[NativeViewport.CAD_EXTRUDE_TIP_X];
        final float ty = (float) tool[NativeViewport.CAD_EXTRUDE_TIP_Y];
        final float bx = 2.0f * lx - tx;
        final float by = 2.0f * ly - ty;
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
     * A finger's tap at a viewport pixel, dispatched to the WINDOW: whatever
     * stands over the viewport there (the HUD, a proxy) receives it first, as
     * it would from a real touch screen.
     */
    private void realTap(final float x, final float y) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final View root = activity.getWindow().getDecorView();
            final int[] vp = new int[2];
            final int[] rp = new int[2];
            viewport.getLocationInWindow(vp);
            root.getLocationInWindow(rp);
            final float wx = x + vp[0] - rp[0];
            final float wy = y + vp[1] - rp[1];
            final long down = SystemClock.uptimeMillis();
            dispatch(root, down, down, MotionEvent.ACTION_DOWN, wx, wy);
            dispatch(root, down, down + 40L, MotionEvent.ACTION_UP, wx, wy);
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

    private void assertNoLegacyOverlap(String where) {
        final int status = NativeViewport.sketchLastStatus();
        assertNotEquals(where + ": no legacy overlap refusal", NativeViewport.CAD_OVERLAPPING_REGIONS,
                status);
        assertNotEquals(where + ": no legacy hole refusal", NativeViewport.CAD_OVERLAPPING_HOLES,
                status);
        final String line = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View message = workspace.findViewById(R.id.status_message);
            return message instanceof android.widget.TextView
                    ? ((android.widget.TextView) message).getText().toString() : "";
        });
        assertTrue(where + ": the status line names no overlap: " + line,
                !line.contains(activityString(R.string.status_cad_overlapping_regions))
                        && !line.contains(activityString(R.string.status_cad_overlapping_holes)));
    }

    private String activityString(int id) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> activity.getString(id));
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

    private static int selectedCount() {
        return (int) toolState()[NativeViewport.CAD_EXTRUDE_SELECTED_REGIONS];
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

    // -----------------------------------------------------------------------
    // Evidence
    // -----------------------------------------------------------------------

    private void capture(String name) {
        settleLayout();
        final Bitmap frame =
                InstrumentationRegistry.getInstrumentation().getUiAutomation().takeScreenshot();
        if (frame == null) {
            fact("capture." + name, "unavailable");
            return;
        }
        final File png = new File(outDir, name + ".png");
        try (FileOutputStream out = new FileOutputStream(png)) {
            frame.compress(Bitmap.CompressFormat.PNG, 100, out);
        } catch (IOException error) {
            fact("capture." + name, "write_failed");
            return;
        }
        fact("capture." + name, png.getName() + " " + frame.getWidth() + "x" + frame.getHeight());
    }

    private void fact(String key, Object value) {
        final String line = key + "=" + value;
        facts.add(line);
        Log.i(TAG, "CADV6S2CORR_DEVICE " + line);
    }

    private void writeFacts() {
        final File file = new File(outDir, "facts-" + System.nanoTime() + ".txt");
        try (PrintWriter out = new PrintWriter(file, "UTF-8")) {
            for (String line : facts) {
                out.println(line);
            }
        } catch (IOException error) {
            Log.w(TAG, "CADV6S2CORR_DEVICE facts not written: " + error);
        }
    }
}
