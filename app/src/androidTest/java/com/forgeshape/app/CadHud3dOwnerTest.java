package com.forgeshape.app;

import static com.forgeshape.app.SketchTestSupport.dragSketch;
import static com.forgeshape.app.SketchTestSupport.selectTool;
import static com.forgeshape.app.SketchTestSupport.sketchState;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.graphics.Bitmap;
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
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * `CAD-V6-S2-OWNER-CORRECTION-E2E-R1` (HUD3D) on the device: the extrude
 * action DOCK is a world rectangle on the extrusion axis, projected and drawn
 * by Android into its four corners.
 *
 * <p>It stays attached to the arrow through a slow orbit (no side teleport, no
 * rescue slide), at a viewport edge it stays where the geometry puts it or
 * hides WHOLE, it fades and hides looking down the axis without turning over,
 * a real tap on it opens the full-size palette, a cell beside it stays
 * tappable, and the dimension value stays on its leader throughout. Asserted
 * from native truth and view state; the screenshots are evidence, never an
 * assertion.
 */
@RunWith(AndroidJUnit4.class)
public final class CadHud3dOwnerTest {

    private static final String TAG = "ForgeShape";

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private byte[] baselineProject;
    private int projectionBefore;
    private File outDir;
    private final List<String> facts = new ArrayList<>();
    private int marks;

    @Before
    public void setUp() {
        outDir = new File(InstrumentationRegistry.getInstrumentation().getTargetContext()
                .getExternalFilesDir(null), "evidence/cad-v6-s2-hud3d");
        //noinspection ResultOfMethodCallIgnored
        outDir.mkdirs();
        resetToBaselineConstruction(rule.getScenario());
        baselineProject = NativeViewport.encodeProject();
        projectionBefore = NativeViewport.projectionMode();
    }

    @After
    public void tearDown() {
        writeFacts();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.sketchCancel();
            NativeViewport.setProjectionMode(projectionBefore);
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
    // DEV-HUD3D-01 + 06: a slow orbit; the dimension value stays on its leader
    // =======================================================================

    @Test
    public void devHud3d01_06_a_slow_orbit_keeps_the_dock_attached_and_the_value_on_its_leader() {
        stageExtrusion();
        int shown = 0;
        int valueChecked = 0;
        float largestJumpDp = 0.0f;
        float largestReachDp = 0.0f;
        float[] previousOffset = null;
        float[] previousQuad = null;
        int turns = 0;
        final List<String> problems = new ArrayList<>();
        for (int step = 0; step <= 125; step++) {
            final float yaw = 0.2f + 0.05f * step;
            setCamera(yaw, 0.5f, 9.0f);
            final Frame f = frame();
            if (step == 14) capture("oblique_left");
            if (step == 50) capture("oblique_right");
            if (f.valueWhy != null) {
                problems.add("step " + step + " value: " + f.valueWhy);
            } else if (f.valueShown) {
                valueChecked++;
            }
            if (!f.dock.visible) {
                fact("orbit.step" + step, "hidden reason=" + f.dock.hiddenReason);
                previousOffset = null;
                previousQuad = null;
                continue;
            }
            shown++;
            if (f.dockWhy != null) {
                problems.add("step " + step + " dock: " + f.dockWhy);
            }
            final float[] offset = {f.dock.centreX - f.headX, f.dock.centreY - f.headY};
            final float reachDp = (float) Math.hypot(offset[0], offset[1]) / f.density;
            largestReachDp = Math.max(largestReachDp, reachDp);
            if (previousOffset != null) {
                final float jump = (float) Math.hypot(offset[0] - previousOffset[0],
                        offset[1] - previousOffset[1]) / f.density;
                largestJumpDp = Math.max(largestJumpDp, jump);
                final boolean[] wrapped = new boolean[1];
                final float move = quadMove(previousQuad, f.dock.quad, wrapped) / f.density;
                if (wrapped[0]) turns++;
                if (jump > 24.0f || move > 40.0f) {
                    problems.add("step " + step + ": offset jump " + jump + " dp, corner move "
                            + move + " dp");
                }
            }
            previousOffset = offset;
            previousQuad = f.dock.quad.clone();
            fact("orbit.step" + step, "yaw=" + yaw + " reach_dp=" + reachDp + " sine="
                    + f.dock.axisSine + " alpha=" + f.dock.alpha + " quad="
                    + Arrays.toString(f.dock.quad));
        }
        fact("orbit.shown", shown);
        fact("orbit.value_checked", valueChecked);
        fact("orbit.largest_offset_jump_dp", largestJumpDp);
        fact("orbit.largest_reach_dp", largestReachDp);
        fact("orbit.in_place_turns", turns);
        assertTrue("attached through the orbit: " + problems, problems.isEmpty());
        assertTrue("the dock stands for most of a full orbit: " + shown, shown >= 100);
        assertTrue("the value was checked on its leader: " + valueChecked, valueChecked >= 60);
        // No rescue slide: the dock is never far down or away from the point.
        assertTrue("never a 200 dp rescue: " + largestReachDp, largestReachDp < 120.0f);
    }

    // =======================================================================
    // DEV-HUD3D-02: close zoom pushes it off the edge -- it hides, never slides
    // =======================================================================

    @Test
    public void devHud3d02_at_the_viewport_edge_the_dock_stays_on_the_geometry_or_hides_whole() {
        stageExtrusion();
        int shown = 0;
        int hiddenOff = 0;
        final List<String> problems = new ArrayList<>();
        boolean captured = false;
        float distance = 9.0f;
        for (int step = 0; step <= 40; step++) {
            setCamera(0.9f, 0.45f, distance);
            final Frame f = frame();
            if (f.dock.visible) {
                shown++;
                if (f.dockWhy != null) {
                    problems.add("d=" + distance + ": " + f.dockWhy);
                }
                if (!captured && edgeDistanceDp(f) < 40.0f) {
                    capture("near_edge");
                    captured = true;
                }
            } else {
                if (f.dock.hiddenReason == NativeViewport.DOCK_HIDDEN_OFF_VIEWPORT
                        || f.dock.hiddenReason == NativeViewport.DOCK_HIDDEN_BEHIND_EYE) {
                    hiddenOff++;
                }
                if (f.dockShown) {
                    problems.add("d=" + distance + ": hidden by native but its view is shown");
                }
            }
            fact("edge.d" + distance, (f.dock.visible ? "shown" : "hidden reason="
                    + f.dock.hiddenReason) + " head=" + f.headX + "," + f.headY);
            distance *= 0.93f;
        }
        if (!captured) {
            capture("near_edge");
        }
        assertTrue("near an edge the dock stays on the geometry: " + problems,
                problems.isEmpty());
        assertTrue("it was drawn while it fit: " + shown, shown >= 3);
        assertTrue("and hidden whole once it did not: " + hiddenOff, hiddenOff >= 1);
    }

    // =======================================================================
    // DEV-HUD3D-03: looking down the axis it fades and hides, never flips
    // =======================================================================

    @Test
    public void devHud3d03_looking_down_the_axis_fades_then_hides_without_a_flip() {
        stageExtrusion();
        float lastAlpha = 2.0f;
        float[] previousTop = null;
        boolean sawNearAxis = false;
        boolean sawFade = false;
        boolean captured = false;
        final List<String> problems = new ArrayList<>();
        // yaw 0, pitch 0 looks straight down the XY sketch's +Z axis.
        for (int step = 0; step <= 50; step++) {
            final float pitch = 1.0f - 0.02f * step;
            setCamera(0.0f, pitch, 9.0f);
            final Frame f = frame();
            fact("axis.pitch" + pitch, "visible=" + f.dock.visible + " reason="
                    + f.dock.hiddenReason + " sine=" + f.dock.axisSine + " alpha="
                    + f.dock.alpha);
            if (!f.dock.visible) {
                if (f.dock.hiddenReason == NativeViewport.DOCK_HIDDEN_NEAR_AXIS) {
                    sawNearAxis = true;
                    if (!captured) {
                        capture("near_axis");
                        captured = true;
                    }
                }
                if (f.dockShown) {
                    problems.add("pitch " + pitch + ": hidden by native but shown");
                }
                continue;
            }
            if (sawNearAxis) {
                problems.add("pitch " + pitch + ": shown again after hiding on the way in");
            }
            if (f.dock.alpha > lastAlpha + 1e-4f) {
                problems.add("pitch " + pitch + ": alpha rose " + lastAlpha + " -> "
                        + f.dock.alpha);
            }
            lastAlpha = f.dock.alpha;
            sawFade = sawFade || f.dock.alpha < 1.0f;
            final float[] top = {f.dock.quad[2] - f.dock.quad[0], f.dock.quad[3] - f.dock.quad[1]};
            if (previousTop != null && top[0] * previousTop[0] + top[1] * previousTop[1] < 0.0f) {
                problems.add("pitch " + pitch + ": the badge turned over");
            }
            previousTop = top;
        }
        assertTrue("toward the axis: " + problems, problems.isEmpty());
        assertTrue("it hid near the axis", sawNearAxis);
        assertTrue("after fading", sawFade);
    }

    // =======================================================================
    // DEV-HUD3D-04: a real tap on the dock opens the full-size palette
    // =======================================================================

    @Test
    public void devHud3d04_a_real_tap_on_the_dock_opens_the_full_size_palette() {
        stageExtrusion();
        setCamera(0.8f, 0.5f, 9.0f);
        final Frame f = frame();
        assertTrue("the dock is drawn", f.dock.visible);
        assertNull(f.dockWhy, f.dockWhy);
        final String mark = mark();
        realTap(f.dock.centreX, f.dock.centreY);
        final List<String> tokens = tokensSince(mark);
        fact("palette.tap_tokens", tokens);
        assertTrue("attributed to the dock: " + tokens,
                tokens.contains("FORGESHAPE_CAD_HUD_TOUCH:dock"));
        assertFalse("never a sketch tap: " + tokens,
                tokens.toString().contains("FORGESHAPE_SKETCH_TAP"));
        capture("palette_open");
        final float[] symmetric = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final CadExtrudeCanvasView canvas = workspace.cadExtrudeCanvas();
            assertTrue("the palette opened", canvas.actionPaletteOpen());
            final float density = activity.getResources().getDisplayMetrics().density;
            final int floor = Math.round(48f * density) - 1;
            for (int id : new int[]{R.id.cad_extrude_extent_one_side,
                    R.id.cad_extrude_extent_symmetric, R.id.cad_extrude_extent_two_sides,
                    R.id.cad_extrude_flip}) {
                final View control = canvas.findViewById(id);
                assertTrue("shown: " + id, control.isShown());
                assertTrue("a full 48 dp target: " + id,
                        control.getWidth() >= floor && control.getHeight() >= floor);
                assertEquals("never scaled: " + id, 1.0f,
                        control.getScaleX() * control.getScaleY(), 0.0f);
            }
            return viewportPoint(workspace, canvas.findViewById(R.id.cad_extrude_extent_symmetric));
        });
        realTap(symmetric[0], symmetric[1]);
        assertEquals("the palette chose Symmetric", NativeViewport.EXTENT_SYMMETRIC,
                (int) toolState()[NativeViewport.CAD_EXTRUDE_EXTENT]);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertFalse("and closed", workspace.cadExtrudeCanvas().actionPaletteOpen());
            return null;
        });
        // Back to One Side and Flip, both through the dock again.
        final Frame g = frame();
        assertTrue(g.dock.visible);
        realTap(g.dock.centreX, g.dock.centreY);
        final float[] oneSide = onWorkspace(rule.getScenario(), (activity, workspace) ->
                viewportPoint(workspace, workspace.cadExtrudeCanvas()
                        .findViewById(R.id.cad_extrude_extent_one_side)));
        realTap(oneSide[0], oneSide[1]);
        assertEquals(NativeViewport.EXTENT_ONE_SIDE,
                (int) toolState()[NativeViewport.CAD_EXTRUDE_EXTENT]);
        final int direction = (int) toolState()[NativeViewport.CAD_EXTRUDE_DIRECTION];
        final Frame h = frame();
        assertTrue(h.dock.visible);
        realTap(h.dock.centreX, h.dock.centreY);
        final float[] flip = onWorkspace(rule.getScenario(), (activity, workspace) ->
                viewportPoint(workspace, workspace.cadExtrudeCanvas()
                        .findViewById(R.id.cad_extrude_flip)));
        realTap(flip[0], flip[1]);
        assertTrue("Flip reached from the dock reversed the side",
                (int) toolState()[NativeViewport.CAD_EXTRUDE_DIRECTION] != direction);
    }

    // =======================================================================
    // DEV-HUD3D-05: a cell just outside the dock's claim stays tappable
    // =======================================================================

    @Test
    public void devHud3d05_a_cell_beside_the_dock_stays_tappable() {
        stageExtrusion();
        setCamera(0.8f, 0.5f, 9.0f);
        final Frame f = frame();
        assertTrue("the dock is drawn", f.dock.visible);
        final float floor = 48.0f * f.density;
        // A point of the rectangle's interior that projects into a band just
        // outside the dock's claim -- the floor square or the quad, whichever
        // reaches further -- yet inside its view box when the box is larger.
        float[] target = null;
        final float[] at = new float[2];
        String claimedBy = "";
        search:
        for (double u = -1.8; u <= 1.8; u += 0.02) {
            for (double v = -1.8; v <= 1.8; v += 0.02) {
                if (!NativeViewport.sketchScreenPoint(u, v, at)) continue;
                if (CadHud3dPresentation.claims(f.dock, at[0], at[1], floor)) continue;
                final float dx = Math.max(Math.abs(at[0] - f.dock.centreX) - floor / 2, 0.0f);
                final float dy = Math.max(Math.abs(at[1] - f.dock.centreY) - floor / 2, 0.0f);
                final float gap = (float) Math.hypot(dx, dy);
                if (gap >= 4.0f * f.density && gap <= 10.0f * f.density
                        && shaftDistance(f, at[0], at[1]) > 30.0f * f.density) {
                    target = new float[]{at[0], at[1]};
                    claimedBy = "uv=" + u + "," + v;
                    break search;
                }
            }
        }
        assertNotNull("a rectangle point just outside the dock's claim", target);
        fact("beside.target", target[0] + "," + target[1] + " " + claimedBy + " dock_centre="
                + f.dock.centreX + "," + f.dock.centreY);
        final double before = toolState()[NativeViewport.CAD_EXTRUDE_SELECTED_REGIONS];
        final String mark = mark();
        realTap(target[0], target[1]);
        final List<String> tokens = tokensSince(mark);
        fact("beside.tokens", tokens);
        assertTrue("the tap reached the sketch and resolved: " + tokens,
                tokens.contains("FORGESHAPE_SKETCH_TAP:resolved"));
        assertFalse("the dock did not take it: " + tokens,
                tokens.contains("FORGESHAPE_CAD_HUD_TOUCH:dock"));
        assertTrue("the region toggled", toolState()[NativeViewport.CAD_EXTRUDE_SELECTED_REGIONS]
                != before);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertFalse("the palette did not open",
                    workspace.cadExtrudeCanvas().actionPaletteOpen());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // Staging
    // -----------------------------------------------------------------------

    /** A 3.6 m square on XY, finished, extruded 0.6 m, seen in perspective. */
    private void stageExtrusion() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openAddPrimitive(workspace);
            workspace.addPrimitivePalette().findViewById(R.id.sketch_plane_by_name).performClick();
            workspace.addPrimitivePalette().findViewById(R.id.sketch_plane_xy).performClick();
            return null;
        });
        settleLayout();
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), -1.8, -1.8, 1.8, 1.8);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.finish_sketch).performClick();
            return null;
        });
        settleLayout();
        assertEquals("Finish succeeds: " + NativeViewport.sketchLastStatus(),
                NativeViewport.SKETCH_READY, sketchState());
        assertEquals("the one region is chosen", 1.0,
                toolState()[NativeViewport.CAD_EXTRUDE_SELECTED_REGIONS], 0.0);
        assertEquals(NativeViewport.CAD_OK, NativeViewport.sketchSetExtrude(0.6,
                (int) toolState()[NativeViewport.CAD_EXTRUDE_DIRECTION]));
        assertEquals(NativeViewport.PROJECTION_PERSPECTIVE,
                NativeViewport.setProjectionMode(NativeViewport.PROJECTION_PERSPECTIVE));
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

    private static double[] toolState() {
        final double[] tool = new double[NativeViewport.CAD_EXTRUDE_SIZE];
        NativeViewport.cadExtrudeToolState(tool);
        return tool;
    }

    /** One frame's dock, its checks, and the value's check, read together. */
    private static final class Frame {
        CadHud3dPresentation.Dock dock;
        String dockWhy;
        boolean dockShown;
        boolean valueShown;
        String valueWhy;
        float headX;
        float headY;
        float density;
        float viewportW;
        float viewportH;
        double[] tool;
    }

    private Frame frame() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final Frame f = new Frame();
            f.density = activity.getResources().getDisplayMetrics().density;
            final CadExtrudeCanvasView canvas = workspace.cadExtrudeCanvas();
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            f.viewportW = viewport.getWidth();
            f.viewportH = viewport.getHeight();
            f.tool = toolState();
            f.dock = CadHud3dPresentation.fromToolState(f.tool, CadHudPresentation.hitPx(f.density));
            f.headX = (float) f.tool[NativeViewport.CAD_EXTRUDE_HEAD_X];
            f.headY = (float) f.tool[NativeViewport.CAD_EXTRUDE_HEAD_Y];
            f.dockShown = canvas.findViewById(R.id.cad_extrude_panel).isShown();
            f.dockWhy = f.dock.visible
                    ? CadLeaderHudChecks.dockAtArrow(f.tool, canvas, viewport, f.density) : null;
            final TextView value = canvas.findViewById(R.id.cad_extrude_depth_value);
            f.valueShown = value.isShown();
            if (f.valueShown && CadLeaderHudChecks.leader(f.tool, false) != null) {
                f.valueWhy = CadLeaderHudChecks.valueOnLeader(f.tool, value, viewport, f.density,
                        false);
            }
            return f;
        });
    }

    private static float edgeDistanceDp(Frame f) {
        float nearest = Float.MAX_VALUE;
        for (int i = 0; i < 4; i++) {
            final float x = f.dock.quad[2 * i];
            final float y = f.dock.quad[2 * i + 1];
            nearest = Math.min(nearest, Math.min(Math.min(x, f.viewportW - x),
                    Math.min(y, f.viewportH - y)));
        }
        return nearest / f.density;
    }

    /** px from the drawn shaft (base to tip). */
    private static float shaftDistance(Frame f, float x, float y) {
        final float tx = (float) f.tool[NativeViewport.CAD_EXTRUDE_TIP_X];
        final float ty = (float) f.tool[NativeViewport.CAD_EXTRUDE_TIP_Y];
        final float bx = 2.0f * (float) f.tool[NativeViewport.CAD_EXTRUDE_LABEL_X] - tx;
        final float by = 2.0f * (float) f.tool[NativeViewport.CAD_EXTRUDE_LABEL_Y] - ty;
        final float ex = f.headX;
        final float ey = f.headY;
        final float dx = ex - bx;
        final float dy = ey - by;
        final float len2 = dx * dx + dy * dy;
        float t = len2 > 0.0f ? ((x - bx) * dx + (y - by) * dy) / len2 : 0.0f;
        t = Math.max(0.0f, Math.min(1.0f, t));
        return (float) Math.hypot(x - (bx + dx * t), y - (by + dy * t));
    }

    /** The largest corner move between two quads, allowing the 180-degree in-place turn. */
    private static float quadMove(float[] a, float[] b, boolean[] wrapped) {
        float straight = 0.0f;
        float turned = 0.0f;
        for (int i = 0; i < 4; i++) {
            final int k = (i + 2) % 4;
            straight = Math.max(straight, (float) Math.hypot(a[2 * i] - b[2 * i],
                    a[2 * i + 1] - b[2 * i + 1]));
            turned = Math.max(turned, (float) Math.hypot(a[2 * i] - b[2 * k],
                    a[2 * i + 1] - b[2 * k + 1]));
        }
        wrapped[0] = turned < straight;
        return Math.min(straight, turned);
    }

    /** A view's centre in viewport pixels. */
    private static float[] viewportPoint(EditorWorkspaceView workspace, View view) {
        final int[] at = new int[2];
        final int[] vp = new int[2];
        view.getLocationInWindow(at);
        workspace.findViewById(R.id.viewport_surface).getLocationInWindow(vp);
        return new float[]{at[0] - vp[0] + view.getWidth() * 0.5f,
                at[1] - vp[1] + view.getHeight() * 0.5f};
    }

    /** One still finger at a viewport point, through the WINDOW's dispatch. */
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
            dispatch(root, down, down + 16L, MotionEvent.ACTION_MOVE, wx + 2.0f, wy - 1.0f);
            dispatch(root, down, down + 40L, MotionEvent.ACTION_UP, wx + 2.0f, wy - 1.0f);
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
    // Evidence
    // -----------------------------------------------------------------------

    private String mark() {
        final String marker = "HUD3D_MARK_" + System.nanoTime() + "_" + (marks++);
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

    /**
     * Saves the screen after the renderer presented a few new frames. A
     * timeout is recorded, never asserted: the picture is evidence only.
     */
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
        Log.i(TAG, "HUD3D_DEVICE " + line);
    }

    private void writeFacts() {
        final File file = new File(outDir, "facts-" + System.nanoTime() + ".txt");
        try (PrintWriter out = new PrintWriter(file, "UTF-8")) {
            for (String line : facts) {
                out.println(line);
            }
        } catch (IOException error) {
            Log.w(TAG, "HUD3D_DEVICE facts not written: " + error);
        }
    }
}
