package com.forgeshape.app;

import static com.forgeshape.app.SketchTestSupport.dragSketch;
import static com.forgeshape.app.SketchTestSupport.selectTool;
import static com.forgeshape.app.SketchTestSupport.sketchState;
import static com.forgeshape.app.SketchTestSupport.tapWorld;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.SystemClock;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

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
 * `CADVS-02`: the OWNER's phone findings, reproduced and MEASURED on the unfixed
 * product before `CAD-VERTICAL-SLICE-R1` changes any behaviour.
 *
 * <p>Test-only and read-only: it drives the product exactly as a user would
 * (semantic view ids, native projections for every viewport pixel) and records
 * what it finds -- bounding boxes, the share of the viewport each CAD surface
 * covers, which profile Finish Sketch picks for a rectangle around a centred
 * circle, how many bodies a later sketch makes, and whether Add or Cut can be
 * chosen at all. Each value is written as a {@code CADVS_BEFORE} log line and
 * into {@code files/evidence/cad-vertical-slice-r1-before/facts.txt} beside the
 * captures, which the device CI job pulls into its evidence.
 *
 * <p>The assertions state the defects as they ARE, so the run proves the
 * reproduction rather than merely describing it. The class is retired by the
 * commit that fixes them; its evidence stays in
 * {@code artifacts/cad-vertical-slice-r1/before/}.
 */
@RunWith(AndroidJUnit4.class)
public final class CadVerticalSliceBeforeTest {

    private static final String TAG = "ForgeShape";

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private byte[] baselineProject;
    private File outDir;
    private final List<String> facts = new ArrayList<>();

    @Before
    public void setUp() {
        outDir = new File(InstrumentationRegistry.getInstrumentation().getTargetContext()
                .getExternalFilesDir(null), "evidence/cad-vertical-slice-r1-before");
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
            return null;
        });
        resetToBaselineConstruction(rule.getScenario());
    }

    // -----------------------------------------------------------------------
    // 1. The extrude HUD's footprint, in every extent mode
    // -----------------------------------------------------------------------

    @Test
    public void before01_extrudeHudFootprint() {
        beginSketchXy();
        drawRectangle(2.0, 1.0);
        finishSketch();
        measureCadSurfaces("one_side");
        capture("01_ready_one_side");

        tapExtent(R.id.cad_extrude_extent_symmetric);
        measureCadSurfaces("symmetric");
        capture("02_ready_symmetric");

        tapExtent(R.id.cad_extrude_extent_two_sides);
        measureCadSurfaces("two_sides");
        capture("03_ready_two_sides");

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View canvas = workspace.cadExtrudeCanvas();
            final View chip = canvas.findViewById(R.id.cad_extrude_extent_one_side);
            assertNotNull(chip);
            final float density = activity.getResources().getDisplayMetrics().density;
            fact("extent_chip_visual_height_dp", chip.getHeight() * chip.getScaleY() / density);
            // The defect as it IS: the extent chips are authored at 60 dp and
            // are text, not icons.
            assertTrue("the extent chip is a text pill",
                    chip instanceof TextView && ((TextView) chip).getText().length() > 0);
            assertTrue("drawn at least 48 dp tall",
                    chip.getHeight() * chip.getScaleY() >= 47f * density);
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // 2. A rectangle around a centred circle: which profile does Finish pick?
    // -----------------------------------------------------------------------

    @Test
    public void before02_rectangleWithCentredCircleAutoSelectsTheDisk() {
        beginSketchXy();
        drawRectangle(4.0, 3.0);
        selectTool(rule.getScenario(), R.id.tool_rail_circle);
        dragSketch(rule.getScenario(), 0.0, 0.0, 0.8, 0.0);
        final double[] editing = sketchStateArray();
        fact("entities", editing[NativeViewport.SKETCH_ENTITY_COUNT]);
        finishSketch();
        capture("04_rectangle_circle_after_finish");
        final double[] ready = sketchStateArray();
        final long chosen = (long) ready[NativeViewport.SKETCH_CHOSEN_PROFILE];
        fact("profile_count", ready[NativeViewport.SKETCH_PROFILE_COUNT]);
        fact("chosen_profile_anchor", chosen);
        final double[] info = new double[3];
        assertTrue(NativeViewport.sketchProfileInfo(chosen, info));
        fact("chosen_profile_kind", info[0]);
        fact("chosen_profile_area_m2", info[1] > 0 ? info[2] : info[2]);
        // The defect as it IS: exactly one profile survives (the rectangle is
        // refused as nested), and it is chosen without asking: the circle.
        assertEquals("one profile survives", 1.0, ready[NativeViewport.SKETCH_PROFILE_COUNT], 0.0);
        assertEquals("and it is the circle (entity 2)", 2L, chosen);
        assertEquals("of kind circle", NativeViewport.SKETCH_PROFILE_KIND_CIRCLE, (int) info[0]);
        measureCadSurfaces("rect_circle_ready");
    }

    // -----------------------------------------------------------------------
    // 3. A later sketch on the body always makes ANOTHER body
    // -----------------------------------------------------------------------

    @Test
    public void before03_laterSketchOnTheBodyCreatesASecondBody() {
        final int bodiesBefore = NativeViewport.sceneBodyCount();
        // On XZ, so the far cap faces +Y and is seen from above -- the pose the
        // CAD-A3 evidence already uses to aim at a producer's cap.
        beginSketch(R.id.sketch_plane_xz);
        drawRectangle(2.0, 2.0);
        finishSketch();
        final long base = extrude();
        assertTrue("a base CAD body", base != NativeViewport.NO_OBJECT);
        final int afterBase = NativeViewport.sceneBodyCount();
        fact("bodies_before", bodiesBefore);
        fact("bodies_after_base", afterBase);
        fact("base_id", base);

        // A second sketch ON the base body's far cap (y = 1).
        assertTrue(NativeViewport.debugSetCameraPose(0.7f, 0.9f, 8.0f));
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(NativeViewport.supportChooserBegin(true));
            return null;
        });
        settleLayout();
        tapWorld(rule.getScenario(), 0.0, 1.0, 0.0);
        fact("chooser_selected_kind", NativeViewport.supportChooserSelectedKind());
        tapWorld(rule.getScenario(), 0.0, 1.0, 0.0);
        assertEquals("a face-supported sketch is open", NativeViewport.SKETCH_EDITING, sketchState());
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), -0.4, -0.4, 0.4, 0.4);
        finishSketch();
        noAddOrCutAnywhere("face_sketch_ready");
        capture("05_face_sketch_ready");
        final long second = extrude();
        final int afterSecond = NativeViewport.sceneBodyCount();
        fact("second_id", second);
        fact("bodies_after_second", afterSecond);
        capture("06_after_second_extrude");
        // The defect as it IS: the later sketch became a second SceneObject.
        assertTrue("a second id", second != NativeViewport.NO_OBJECT && second != base);
        assertEquals("the Objects list grew by one", afterBase + 1, afterSecond);
    }

    // -----------------------------------------------------------------------
    // 4. Add and Cut cannot be chosen
    // -----------------------------------------------------------------------

    @Test
    public void before04_addAndCutCannotBeChosen() {
        beginSketchXy();
        drawRectangle(2.0, 1.0);
        finishSketch();
        noAddOrCutAnywhere("world_sketch_ready");
    }

    // -----------------------------------------------------------------------
    // helpers
    // -----------------------------------------------------------------------

    private void noAddOrCutAnywhere(String label) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View canvas = workspace.cadExtrudeCanvas();
            final TextView badge = canvas.findViewById(R.id.cad_extrude_operation);
            assertNotNull(badge);
            fact(label + ".operation_badge", badge.getText());
            fact(label + ".operation_badge_clickable", badge.isClickable());
            final boolean anyAddCut = containsText(workspace, "Add") || containsText(workspace, "Cut")
                    || containsText(workspace, "Join") || containsText(workspace, "Subtract");
            fact(label + ".add_or_cut_control_present", anyAddCut);
            assertEquals("the badge reads New Body",
                    activity.getString(R.string.extrude_operation_new_body), badge.getText().toString());
            assertTrue("and is not a control", !badge.isClickable());
            assertTrue("no Add / Cut control exists", !anyAddCut);
            return null;
        });
    }

    private void measureCadSurfaces(String label) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final Rect vp = screenRect(viewport);
            final double vpArea = (double) vp.width() * vp.height();
            final float density = activity.getResources().getDisplayMetrics().density;
            fact(label + ".viewport", vp.toShortString() + " area_px=" + (long) vpArea
                    + " density=" + density);
            final View canvas = workspace.cadExtrudeCanvas();
            final View value = canvas.findViewById(R.id.cad_extrude_depth_value);
            final View cluster = value == null ? null : (View) value.getParent();
            record(label, "extrude_cluster", cluster, vpArea);
            record(label, "extent_one_side", canvas.findViewById(R.id.cad_extrude_extent_one_side), vpArea);
            record(label, "extent_symmetric", canvas.findViewById(R.id.cad_extrude_extent_symmetric), vpArea);
            record(label, "extent_two_sides", canvas.findViewById(R.id.cad_extrude_extent_two_sides), vpArea);
            record(label, "exact_value", value, vpArea);
            record(label, "operation_label", canvas.findViewById(R.id.cad_extrude_operation), vpArea);
            record(label, "flip", canvas.findViewById(R.id.cad_extrude_flip), vpArea);
            final View second = canvas.findViewById(R.id.cad_extrude_second_value);
            record(label, "second_value", second, vpArea);
            record(label, "second_cluster", second == null ? null : (View) second.getParent(), vpArea);
            record(label, "orientation_navigator", workspace.sketchNavigator(), vpArea);
            record(label, "sketch_tool_rail", workspace.findViewById(R.id.tool_rail_scroll), vpArea);
            record(label, "trailing_host", workspace.findViewById(R.id.workspace_trailing_host), vpArea);
            record(label, "precision_panel", workspace.propertyInspector(), vpArea);
            final double[] tool = new double[NativeViewport.CAD_EXTRUDE_SIZE];
            NativeViewport.cadExtrudeToolState(tool);
            fact(label + ".control_scale", tool[NativeViewport.CAD_EXTRUDE_SCALE]);
            fact(label + ".label_anchor_px", tool[NativeViewport.CAD_EXTRUDE_LABEL_X] + ","
                    + tool[NativeViewport.CAD_EXTRUDE_LABEL_Y]);
            if (value != null && value.isShown()) {
                final Rect v = screenRect(value);
                final double dx = v.exactCenterX() - (vp.left + tool[NativeViewport.CAD_EXTRUDE_LABEL_X]);
                final double dy = v.exactCenterY() - (vp.top + tool[NativeViewport.CAD_EXTRUDE_LABEL_Y]);
                fact(label + ".exact_value_to_shaft_anchor_dp", Math.hypot(dx, dy) / density);
            }
            return null;
        });
    }

    private void record(String label, String name, View view, double viewportArea) {
        if (view == null || !view.isShown()) {
            fact(label + "." + name, "absent");
            return;
        }
        final Rect r = screenRect(view);
        final double sx = view.getScaleX();
        final double sy = view.getScaleY();
        final double w = view.getWidth() * sx;
        final double h = view.getHeight() * sy;
        fact(label + "." + name, r.toShortString() + " drawn_px=" + Math.round(w) + "x" + Math.round(h)
                + " scale=" + sx + " viewport_pct=" + String.format(java.util.Locale.ROOT, "%.2f",
                100.0 * w * h / viewportArea));
    }

    private static Rect screenRect(View view) {
        final int[] at = new int[2];
        view.getLocationOnScreen(at);
        return new Rect(at[0], at[1], at[0] + Math.round(view.getWidth() * view.getScaleX()),
                at[1] + Math.round(view.getHeight() * view.getScaleY()));
    }

    private static boolean containsText(View root, String text) {
        if (root instanceof TextView && root.isShown()
                && ((TextView) root).getText().toString().equals(text)) {
            return true;
        }
        if (root instanceof ViewGroup) {
            final ViewGroup group = (ViewGroup) root;
            for (int i = 0; i < group.getChildCount(); ++i) {
                if (containsText(group.getChildAt(i), text)) {
                    return true;
                }
            }
        }
        return false;
    }

    private void beginSketchXy() {
        beginSketch(R.id.sketch_plane_xy);
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

    private long extrude() {
        final long[] created = new long[1];
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int before = NativeViewport.sceneBodyCount();
            workspace.findViewById(R.id.extrude_sketch).performClick();
            created[0] = NativeViewport.sceneBodyCount() > before ? NativeViewport.sceneActiveBodyId()
                    : NativeViewport.NO_OBJECT;
            return null;
        });
        settleLayout();
        return created[0];
    }

    private void tapExtent(int id) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.cadExtrudeCanvas().findViewById(id).performClick();
            return null;
        });
        settleLayout();
    }

    private static double[] sketchStateArray() {
        final double[] state = new double[NativeViewport.SKETCH_STATE_SIZE];
        NativeViewport.sketchState(state);
        return state;
    }

    private void capture(String name) {
        settleLayout();
        SystemClock.sleep(400);
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
        Log.i(TAG, "CADVS_BEFORE " + line);
    }

    private void writeFacts() {
        final File file = new File(outDir, "facts-" + System.nanoTime() + ".txt");
        try (PrintWriter out = new PrintWriter(file, "UTF-8")) {
            for (String line : facts) {
                out.println(line);
            }
        } catch (IOException error) {
            Log.w(TAG, "CADVS_BEFORE facts not written: " + error);
        }
    }
}
