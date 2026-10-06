package com.forgeshape.app;

import static com.forgeshape.app.SketchTestSupport.dragSketch;
import static com.forgeshape.app.SketchTestSupport.drawRectangleAndExtrude;
import static com.forgeshape.app.SketchTestSupport.selectTool;
import static com.forgeshape.app.SketchTestSupport.sketchEntityCount;
import static com.forgeshape.app.SketchTestSupport.sketchState;
import static com.forgeshape.app.SketchTestSupport.sketchViewState;
import static com.forgeshape.app.SketchTestSupport.tapSketch;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.graphics.Rect;
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
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * `MODELING-R1-OWNER-CORRECTION` on the device, on the OWNER's own path and
 * with REAL window touches: the ForgeShape mark at the top-left opens the
 * project drawer; its New Sketch enters the spatial support chooser; a tap on a
 * planar face -- a base cap, and the floor of a Cut's pocket -- opens a sketch
 * there that commits; and the OWNER's inner-area selection, every cell but two
 * inner cells that meet at one node, stays selected and extrudes as one solid
 * instead of being refused "meet only at a point".
 *
 * <p>Every touch is dispatched through the window's decor view, so whatever
 * chrome stands at that pixel receives it first, exactly as from a touch
 * screen. Asserted from native truth (sketch state, support, face handles,
 * candidate and body measures, the debug tap tokens) and the status line's own
 * text, never from a picture. Emulator evidence closes no physical-device gate.
 */
@RunWith(AndroidJUnit4.class)
public final class OwnerProjectShellSketchSupportTest {

    private static final String TAG = "ForgeShape";

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private File outDir;
    private final List<String> facts = new ArrayList<>();
    private int marks;

    @Before
    public void startAtHome() {
        outDir = new File(InstrumentationRegistry.getInstrumentation().getTargetContext()
                .getExternalFilesDir(null), "evidence/modeling-r1-owner-shell-sketch-fix");
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
            if (workspace.projectPopover().isOpen()) {
                workspace.projectPopover().closeImmediately();
            }
            NativeViewport.supportChooserCancel();
            NativeViewport.sketchCancel();
            NativeViewport.enterConstructionMode();
            return null;
        });
        resetToBaselineConstruction(rule.getScenario());
    }

    // =======================================================================
    // DEV-OSS-01: the ForgeShape mark opens the project drawer from the left
    // =======================================================================

    @Test
    public void devOss01_the_forgeshape_mark_opens_a_left_project_drawer() {
        final long body = newCadProjectBlock();
        assertTrue(body > 0);
        final Rect mark = windowRect(R.id.project_actions_button);
        final Rect viewport = windowRect(R.id.viewport_surface);
        final float density = density();
        fact("oss01.mark", mark.toShortString() + " viewport=" + viewport.toShortString()
                + " density=" + density);
        assertTrue("the mark is drawn", shown(R.id.project_actions_button));
        assertTrue("it is in the top-left: " + mark.toShortString(),
                mark.left < viewport.width() / 4 && mark.top < 96 * density);
        assertTrue("its target is at least 48 dp", mark.width() >= Math.round(48 * density) - 1
                && mark.height() >= Math.round(48 * density) - 1);
        onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View button = workspace.findViewById(R.id.project_actions_button);
            assertEquals("the one project door is the mark's own capsule",
                    R.id.toolbar_mark_group, ((View) button.getParent()).getId());
            final View utility = workspace.findViewById(R.id.toolbar_utility_group);
            assertFalse("nothing in the trailing group opens the project",
                    contains(utility, R.id.project_actions_button));
            assertTrue("Display stays trailing", contains(utility, R.id.display_settings_button));
            assertTrue("Hide UI stays trailing", contains(utility, R.id.hide_ui_toggle));
            return null;
        });

        realTap(centre(mark));
        final Rect drawer = windowRect(R.id.project_actions_popover);
        fact("oss01.drawer", drawer.toShortString());
        assertTrue("the drawer is open", drawerOpen());
        assertTrue("it hangs from the mark at the leading edge: " + drawer.toShortString(),
                Math.abs(drawer.left - mark.left) <= Math.round(16 * density)
                        && drawer.top >= mark.bottom - Math.round(2 * density));
        onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View popover = workspace.projectPopover();
            assertEquals("it grows from its leading top corner", 0.0f, popover.getPivotX(), 0.5f);
            assertEquals(0.0f, popover.getPivotY(), 0.5f);
            assertTrue("the drawer opens on the mark and the product's name",
                    workspace.findViewById(R.id.project_drawer_header).isShown());
            return null;
        });
        for (int id : new int[]{R.id.project_new, R.id.project_new_sketch, R.id.project_save,
                R.id.project_open, R.id.project_save_copy, R.id.project_open_file,
                R.id.project_settings}) {
            assertTrue("row " + name(id) + " is reachable", shown(id));
            final Rect row = windowRect(id);
            assertTrue("row " + name(id) + " is a 48 dp target",
                    row.height() >= Math.round(48 * density) - 1);
        }

        // Close from the mark, and open it again.
        realTap(centre(windowRect(R.id.project_actions_button)));
        assertFalse("the mark closes it", drawerOpen());
        realTap(centre(windowRect(R.id.project_actions_button)));
        assertTrue("and opens it again", drawerOpen());
        realTap(centre(windowRect(R.id.project_actions_button)));
        assertFalse(drawerOpen());
    }

    // =======================================================================
    // DEV-OSS-02: New Sketch from the drawer, on a base planar cap
    // =======================================================================

    @Test
    public void devOss02_new_sketch_from_the_drawer_lands_on_a_tapped_base_cap() {
        final long base = newCadProjectBlock();
        openNewSketchFromDrawer();
        setCamera(0.7f, 0.9f, 8.0f);
        // The base's far cap is at z = 1 (an XY sketch extruded 1 along +Z).
        final List<String> chooser = tapWorldTwice(0.5, 0.5, 1.0);
        fact("oss02.chooser", chooser.toString());
        assertEquals("a sketch opens on the tapped face", NativeViewport.SKETCH_EDITING,
                sketchState());
        assertEquals("face-supported", 1.0,
                sketchViewState()[NativeViewport.SKETCH_VIEW_FACE_SUPPORTED], 0.0);
        assertSketchOriginAt(0.0, 0.0, 1.0);

        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), -0.2, -0.2, 0.2, 0.2);
        assertEquals(1, sketchEntityCount());
        finishSketch();
        final int bodies = NativeViewport.sceneBodyCount();
        commitExtrude(-1, "0.25");
        assertEquals("the sketch committed", NativeViewport.SKETCH_INACTIVE, sketchState());
        assertEquals("a dependent body stands on the cap", bodies + 1,
                NativeViewport.sceneBodyCount());
        assertTrue("its placement is the face's", NativeViewport.sceneActiveBodyIsFaceSupportedCad());
        assertNotEquals(base, NativeViewport.sceneActiveBodyId());
    }

    // =======================================================================
    // DEV-OSS-03: New Sketch on the floor a Cut left behind
    // =======================================================================

    @Test
    public void devOss03_new_sketch_lands_on_a_cut_pocket_floor_and_its_feature_commits() {
        final long base = newCadProjectBlock();
        // A 0.6 m square pocket, 0.4 m deep, cut into the far cap.
        openNewSketchFromDrawer();
        setCamera(0.7f, 0.9f, 8.0f);
        tapWorldTwice(0.0, 0.0, 1.0);
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
        drawExactSquare(0.6);
        finishSketch();
        commitExtrude(NativeViewport.OPERATION_CUT, "0.4");
        assertEquals("the Cut changed the SAME body", base, NativeViewport.sceneActiveBodyId());
        assertEquals(2, NativeViewport.cadFeatureCount(base));
        final double afterCut = volume(base);
        assertEquals("the pocket is cut", 2.0 * 2.0 * 1.0 - 0.36 * 0.4, afterCut, 1e-6);

        // The pocket's floor (z = 0.6), seen almost straight down its mouth.
        openNewSketchFromDrawer();
        setCamera(0.08f, 0.12f, 8.0f);
        final List<String> chooser = tapWorldTwice(0.12, 0.12, 0.6);
        fact("oss03.chooser", chooser.toString());
        assertEquals("a sketch opens on the Cut's floor", NativeViewport.SKETCH_EDITING,
                sketchState());
        assertEquals(1.0, sketchViewState()[NativeViewport.SKETCH_VIEW_FACE_SUPPORTED], 0.0);
        // The floor's frame: its centre, facing OUT of the material (+Z).
        assertSketchOriginAt(0.0, 0.0, 0.6);
        drawExactSquare(0.2);
        finishSketch();
        commitExtrude(NativeViewport.OPERATION_ADD, "0.1");
        assertEquals(NativeViewport.SKETCH_INACTIVE, sketchState());
        assertEquals("the boss is a feature of the same body", 3, NativeViewport.cadFeatureCount(base));
        final double[] info = new double[NativeViewport.CAD_FEATURE_INFO_SIZE];
        assertTrue(NativeViewport.cadFeatureInfo(base, 2, info));
        fact("oss03.feature3", Arrays.toString(info));
        assertEquals("it stands on the Cut (feature 2)", 2.0,
                info[NativeViewport.CAD_FEATURE_SUPPORT], 0.0);
        assertEquals("an Add", NativeViewport.OPERATION_ADD,
                (int) info[NativeViewport.CAD_FEATURE_OPERATION]);
        assertEquals("rising into the pocket from its floor", afterCut + 0.04 * 0.1, volume(base),
                1e-6);
    }

    // =======================================================================
    // DEV-OSS-04: the OWNER's inner cells -- every cell but two that meet at
    // one node -- stay selected and extrude
    // =======================================================================

    @Test
    public void devOss04_owner_inner_cells_select_by_real_taps_and_extrude() {
        drawOwnerSketch();
        finishSketch();
        assertEquals("fill mode", 1, NativeViewport.sketchSelectionKind());
        final long[] all = faceHandles();
        fact("oss04.faces", all.length);
        assertTrue("an OWNER-like arrangement: " + all.length, all.length >= 20);
        assertEquals("nothing chosen for the user", 0, selected().length);
        // Nearly face-on, a little off-axis so the extrude arrow is a short
        // line rather than a point on the cells.
        setCamera(0.3f, 0.3f, 9.0f);

        // The node where the circles at (-1, 0) and (0, 0) cross below the
        // axis: four cells meet there. Each is found by a real tap that is
        // then undone, so nothing about the arrangement is assumed.
        final double px = -0.5;
        final double py = -Math.sqrt(0.75);
        final long aOnly = probe(px - 0.07, py);
        final long bOnly = probe(px + 0.07, py);
        final long lens = probe(px, py + 0.07);
        final long outside = probe(px, py - 0.07);
        fact("oss04.node", "aOnly=" + aOnly + " bOnly=" + bOnly + " lens=" + lens + " outside="
                + outside);
        assertEquals("four distinct cells meet at the node", 4,
                new HashSet<>(Arrays.asList(aOnly, bOnly, lens, outside)).size());
        assertEquals(0, selected().length);

        // Every cell but the two inner cells left and right of the node: they
        // are holes of one edge-connected union and they touch at the node.
        final Set<Long> expected = new HashSet<>();
        int taps = 0;
        for (long handle : all) {
            if (handle == aOnly || handle == bOnly) continue;
            final String mark = mark();
            tapFace(handle);
            final List<String> tokens = tokensSince(mark);
            expected.add(handle);
            taps++;
            assertTrue("tap " + taps + " resolved: " + tokens,
                    tokens.contains("FORGESHAPE_SKETCH_TAP:resolved"));
            assertEquals("tap " + taps + " adds exactly its cell", expected.size(),
                    selected().length);
            assertEquals(NativeViewport.CAD_OK, NativeViewport.sketchLastStatus());
        }
        final long[] chosen = selected();
        final Set<Long> actual = new HashSet<>();
        for (long h : chosen) actual.add(h);
        assertEquals("no cell silently dropped", expected, actual);
        assertTrue("16 or more cells", chosen.length >= 16);
        final String touch = string(R.string.status_cad_areas_touch_at_point);
        fact("oss04.selection", "count=" + chosen.length + " candidate=" + candidateStatus()
                + " status=" + statusLine());
        assertEquals("the self-touching union is VALID", NativeViewport.CAD_OK, candidateStatus());
        assertNotEquals("never 'meet only at a point'", touch, statusLine());
        assertEquals(regionsSelected(chosen.length), statusLine());
        assertTrue("Extrude is offered", shown(R.id.extrude_sketch));

        final double area = selectedArea();
        final double depth = depth();
        final double[] preview = candidateMeasure();
        fact("oss04.preview", Arrays.toString(preview) + " area=" + area + " depth=" + depth);
        assertEquals("one solid", 1.0, preview[1], 0.0);
        assertEquals("of exactly the chosen cells", area * depth, preview[0], 2e-3 * area * depth);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.extrude_sketch).performClick();
            return null;
        });
        settleLayout();
        assertEquals("committed: " + statusLine(), NativeViewport.SKETCH_INACTIVE, sketchState());
        assertTrue("the first Extrude created the project", NativeViewport.projectOpen());
        final double[] measure = new double[NativeViewport.CAD_MEASURE_SIZE];
        assertTrue(NativeViewport.cadBodyMeasure(NativeViewport.sceneActiveBodyId(), measure));
        fact("oss04.body", Arrays.toString(measure));
        assertEquals(1.0, measure[NativeViewport.CAD_MEASURE_COMPONENTS], 0.0);
        assertEquals(preview[0], measure[NativeViewport.CAD_MEASURE_VOLUME], 1e-6 * preview[0]);
    }

    // =======================================================================
    // DEV-OSS-05: the rest of the workspace is where it was
    // =======================================================================

    @Test
    public void devOss05_hud_revolve_drafting_and_freeform_surface_entries_still_reachable() {
        // The New Project chooser still offers all four kinds.
        press(R.id.home_new_project);
        for (int id : new int[]{R.id.new_project_cad, R.id.new_project_sculpt,
                R.id.new_project_freeform, R.id.new_project_surface}) {
            assertTrue(name(id) + " is offered", shown(id));
        }
        press(R.id.new_project_cad);
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
        assertTrue("Drafting's Modify is on the sketch", shown(R.id.sketch_modify_toggle)
                || shown(R.id.sketch_modify));
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), -1.0, -1.0, 1.0, 1.0);
        finishSketch();
        assertTrue("the extrude HUD's dock stands at the arrow",
                onWorkspace(rule.getScenario(), (activity, workspace) ->
                        workspace.cadExtrudeCanvas().findViewById(R.id.cad_extrude_panel).isShown()));
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openPrecision(workspace);
            return null;
        });
        settleLayout();
        assertTrue("Revolve… is offered", onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View begin = workspace.sketchEditor().findViewById(R.id.sketch_revolve_begin);
            return begin != null && begin.isShown();
        }));
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.closePrecision(workspace);
            return null;
        });
        settleLayout();
        commitExtrude(-1, "1");
        assertTrue(NativeViewport.projectOpen());
        // Freeform and Surface creation still stand in Add Primitive, beside
        // New Sketch.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openAddPrimitive(workspace);
            return null;
        });
        settleLayout();
        for (int id : new int[]{R.id.add_sketch, R.id.add_surface, R.id.add_freeform_box}) {
            assertTrue(name(id) + " is offered", shown(id));
        }
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.closeAddPrimitive(workspace);
            return null;
        });
        settleLayout();
        // And the drawer's New Sketch is the same door into the chooser.
        openNewSketchFromDrawer();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.supportChooserCancel();
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    // -----------------------------------------------------------------------
    // The project, the drawer and the chooser
    // -----------------------------------------------------------------------

    /** Home -> New Project -> CAD -> a 2 x 2 rectangle extruded 1. */
    private long newCadProjectBlock() {
        press(R.id.home_new_project);
        press(R.id.new_project_cad);
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
        assertFalse("no mark before the project exists", shown(R.id.project_actions_button));
        final long body = drawRectangleAndExtrude(rule.getScenario(), 2.0, 2.0, "1");
        assertTrue(NativeViewport.projectOpen());
        assertTrue("the mark appears with the project", shown(R.id.project_actions_button));
        return body;
    }

    /** The mark, then New Sketch, both by real window taps. */
    private void openNewSketchFromDrawer() {
        realTap(centre(windowRect(R.id.project_actions_button)));
        assertTrue("the drawer is open", drawerOpen());
        assertTrue("New Sketch is offered", shown(R.id.project_new_sketch));
        realTap(centre(windowRect(R.id.project_new_sketch)));
        assertFalse("the drawer closed first", drawerOpen());
        assertTrue("the spatial support chooser is up", NativeViewport.supportChooserActive());
        assertEquals("the status asks for a support", string(R.string.status_support_chooser),
                statusLine());
    }

    /** Aim, then commit, on the pixel a world point projects to -- real taps. */
    private List<String> tapWorldTwice(double x, double y, double z) {
        final float[] at = new float[2];
        assertTrue("the target projects", NativeViewport.debugProjectWorld(x, y, z, at));
        final float[] window = viewportToWindow(at);
        final String mark = mark();
        realTap(window);
        final int kind = NativeViewport.supportChooserSelectedKind();
        fact("chooser.kind", kind + " at " + Arrays.toString(at));
        assertEquals("the tap chose a FACE", NativeViewport.SUPPORT_KIND_FACE, kind);
        realTap(window);
        return tokensSince(mark);
    }

    /** The open sketch's origin projects where the world point does. */
    private void assertSketchOriginAt(double x, double y, double z) {
        final float[] origin = new float[2];
        final float[] world = new float[2];
        assertTrue(NativeViewport.sketchScreenPoint(0.0, 0.0, origin));
        assertTrue(NativeViewport.debugProjectWorld(x, y, z, world));
        fact("sketch.origin", Arrays.toString(origin) + " world=" + Arrays.toString(world));
        assertEquals("the sketch stands on the exact face (x)", world[0], origin[0], 1.5f);
        assertEquals("the sketch stands on the exact face (y)", world[1], origin[1], 1.5f);
    }

    /** A square of side s centred on the sketch origin: a real drag, typed exact. */
    private void drawExactSquare(double side) {
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), -side / 2, -side / 2, side / 2, side / 2);
        assertEquals(1, sketchEntityCount());
        final double[] drawn = new double[NativeViewport.SKETCH_ENTITY_SIZE];
        assertTrue(NativeViewport.sketchSelectedEntity(drawn));
        final long id = (long) drawn[NativeViewport.SKETCH_ENTITY_ID];
        assertEquals(NativeViewport.CAD_OK, applyOnUi(() ->
                NativeViewport.sketchApplyRectangle(id, side, side)));
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

    /** The precision surface: an operation (or -1 to keep it), a depth, Extrude. */
    private void commitExtrude(int operation, String depth) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openPrecision(workspace);
            return null;
        });
        settleLayout();
        if (operation >= 0) {
            final int id = operation == NativeViewport.OPERATION_CUT ? R.id.sketch_operation_cut
                    : operation == NativeViewport.OPERATION_ADD ? R.id.sketch_operation_add
                    : R.id.sketch_operation_new_body;
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                final View chip = workspace.sketchEditor().findViewById(id);
                assertNotNull(chip);
                assertTrue("the operation is offered on a face sketch", chip.isShown());
                chip.performClick();
                return null;
            });
            settleLayout();
        }
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final EditText field = workspace.sketchEditor().findViewById(R.id.field_extrude_depth);
            field.setText(depth);
            workspace.findViewById(R.id.sketch_extrude_commit).performClick();
            return null;
        });
        settleLayout();
        assertEquals("the extrusion committed: " + statusLine() + " last="
                        + NativeViewport.sketchLastStatus(), NativeViewport.SKETCH_INACTIVE,
                sketchState());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.closePrecision(workspace);
            return null;
        });
        settleLayout();
    }

    private static double volume(long body) {
        final double[] measure = new double[NativeViewport.CAD_MEASURE_SIZE];
        assertTrue(NativeViewport.cadBodyMeasure(body, measure));
        return measure[NativeViewport.CAD_MEASURE_VOLUME];
    }

    // -----------------------------------------------------------------------
    // The OWNER-like sketch
    // -----------------------------------------------------------------------

    /**
     * Home -> New Project -> CAD, then: a 6 x 4 rectangle, three overlapping
     * circles of r 1 at (-1, 0), (0, 0), (1, 0), a circle of r 0.5 at (0, 1)
     * crossing them, two more at (-2, -1) r 0.6 and (2, -1.5) r 0.4, a spline
     * running side to side above them, and a 2 x 1 rectangle across the lower
     * right -- each placed by a real drag or real taps, the closed shapes then
     * typed exact. The host derives 21 atomic faces from exactly this sketch,
     * and its all-but-two selection below (19 cells) passes one node twice
     * (`REPRO_BEFORE.md`'s topology).
     */
    private void drawOwnerSketch() {
        press(R.id.home_new_project);
        press(R.id.new_project_cad);
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), -3.0, -2.0, 3.0, 2.0);
        applyRectangle(6.0, 4.0);
        selectTool(rule.getScenario(), R.id.tool_rail_circle);
        for (double[] c : new double[][]{{-1.0, 0.0, 1.0}, {0.0, 0.0, 1.0}, {1.0, 0.0, 1.0},
                {0.0, 1.0, 0.5}, {-2.0, -1.0, 0.6}, {2.0, -1.5, 0.4}}) {
            dragSketch(rule.getScenario(), c[0], c[1], c[0] + c[2], c[1]);
            final double[] drawn = new double[NativeViewport.SKETCH_ENTITY_SIZE];
            assertTrue(NativeViewport.sketchSelectedEntity(drawn));
            assertEquals(NativeViewport.SKETCH_ENTITY_KIND_CIRCLE,
                    (int) drawn[NativeViewport.SKETCH_ENTITY_KIND]);
            fact("owner.circle", Arrays.toString(drawn));
            final long id = (long) drawn[NativeViewport.SKETCH_ENTITY_ID];
            assertEquals(NativeViewport.CAD_OK, applyOnUi(() ->
                    NativeViewport.sketchApplyCircle(id, c[2])));
        }
        selectTool(rule.getScenario(), R.id.tool_rail_spline);
        for (double[] p : new double[][]{{-3.0, 1.5}, {-1.5, 1.0}, {0.5, 1.5}, {1.5, 1.0},
                {3.0, 1.5}, {3.0, 1.5}}) {
            tapSketch(rule.getScenario(), p[0], p[1]);
        }
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), 0.5, -1.5, 2.5, -0.5);
        applyRectangle(2.0, 1.0);
        assertEquals("rectangle, six circles, the spline and a rectangle", 9, sketchEntityCount());
    }

    private void applyRectangle(double width, double height) {
        final double[] drawn = new double[NativeViewport.SKETCH_ENTITY_SIZE];
        assertTrue(NativeViewport.sketchSelectedEntity(drawn));
        assertEquals(NativeViewport.SKETCH_ENTITY_KIND_RECTANGLE,
                (int) drawn[NativeViewport.SKETCH_ENTITY_KIND]);
        final long id = (long) drawn[NativeViewport.SKETCH_ENTITY_ID];
        assertEquals(NativeViewport.CAD_OK, applyOnUi(() ->
                NativeViewport.sketchApplyRectangle(id, width, height)));
    }

    /** Which cell a real tap at a sketch point toggles; the tap is then undone. */
    private long probe(double u, double v) {
        final float[] at = new float[2];
        assertTrue(NativeViewport.sketchScreenPoint(u, v, at));
        final long[] before = selected();
        realTap(viewportToWindow(at));
        final long[] after = selected();
        assertEquals("the probe at (" + u + ", " + v + ") toggled one cell", before.length + 1,
                after.length);
        long hit = -1;
        for (long h : after) {
            if (Arrays.binarySearch(before, h) < 0) hit = h;
        }
        // Taken back by a real tap that toggles exactly that cell.
        tapFace(hit);
        assertEquals("and it was taken back", before.length, selected().length);
        return hit;
    }

    /**
     * One real tap that toggles exactly `handle`, on or off: its native
     * interior point first, then points around it, never within 48 px of the
     * extrude arrow's drawn head (a still tap there is the arrow's); a tap that
     * toggled another cell is taken back before the next is tried.
     */
    private void tapFace(long handle) {
        final double[] info = new double[NativeViewport.SKETCH_REGION_INFO_SIZE];
        assertTrue(NativeViewport.sketchProfileInfo(handle, info));
        assertTrue("cell " + handle + " is on screen",
                info[NativeViewport.SKETCH_REGION_ON_SCREEN] != 0.0);
        final float x = (float) info[NativeViewport.SKETCH_REGION_SCREEN_X];
        final float y = (float) info[NativeViewport.SKETCH_REGION_SCREEN_Y];
        final float[][] tries = {{0, 0}, {12, 0}, {-12, 0}, {0, 12}, {0, -12}, {20, 20},
                {-20, -20}, {20, -20}, {-20, 20}, {30, 0}, {-30, 0}, {0, 30}, {0, -30}};
        for (float[] d : tries) {
            final float px = x + d[0];
            final float py = y + d[1];
            if (headDistance(px, py) < 48.0f) continue;
            final long[] before = selected();
            final boolean wasOn = Arrays.binarySearch(before, handle) >= 0;
            realTap(viewportToWindow(new float[]{px, py}));
            final long[] after = selected();
            final boolean isOn = Arrays.binarySearch(after, handle) >= 0;
            if (isOn != wasOn && after.length == before.length + (wasOn ? -1 : 1)) {
                return;
            }
            if (!Arrays.equals(after, before)) {
                // Another cell answered: take that tap back.
                realTap(viewportToWindow(new float[]{px, py}));
                assertTrue("a stray toggle is undone", Arrays.equals(before, selected()));
            }
        }
        throw new AssertionError("cell " + handle + " could not be tapped at its interior");
    }

    /** px from the extrude arrow's drawn head; infinite while there is none. */
    private static float headDistance(float x, float y) {
        final double[] tool = new double[NativeViewport.CAD_EXTRUDE_SIZE];
        NativeViewport.cadExtrudeToolState(tool);
        if (tool[NativeViewport.CAD_EXTRUDE_ACTIVE] == 0.0
                || tool[NativeViewport.CAD_EXTRUDE_HEAD_ON_SCREEN] == 0.0) {
            return Float.POSITIVE_INFINITY;
        }
        return (float) Math.hypot(x - tool[NativeViewport.CAD_EXTRUDE_HEAD_X],
                y - tool[NativeViewport.CAD_EXTRUDE_HEAD_Y]);
    }

    // -----------------------------------------------------------------------
    // Real window touches
    // -----------------------------------------------------------------------

    /** One still finger through the WINDOW at a window pixel. */
    private void realTap(final float[] window) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View root = activity.getWindow().getDecorView();
            final long down = SystemClock.uptimeMillis();
            dispatch(root, down, down, MotionEvent.ACTION_DOWN, window[0], window[1]);
            dispatch(root, down, down + 40L, MotionEvent.ACTION_UP, window[0], window[1]);
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

    /** A viewport pixel in the decor view's coordinates. */
    private float[] viewportToWindow(float[] at) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final View root = activity.getWindow().getDecorView();
            final int[] vp = new int[2];
            final int[] rp = new int[2];
            viewport.getLocationInWindow(vp);
            root.getLocationInWindow(rp);
            return new float[]{at[0] + vp[0] - rp[0], at[1] + vp[1] - rp[1]};
        });
    }

    /** A view's bounds in the decor view's coordinates. */
    private Rect windowRect(int id) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View view = activity.findViewById(id);
            assertNotNull(resName(activity, id) + " exists", view);
            final View root = activity.getWindow().getDecorView();
            final int[] at = new int[2];
            final int[] rp = new int[2];
            view.getLocationInWindow(at);
            root.getLocationInWindow(rp);
            return new Rect(at[0] - rp[0], at[1] - rp[1], at[0] - rp[0] + view.getWidth(),
                    at[1] - rp[1] + view.getHeight());
        });
    }

    private static float[] centre(Rect r) {
        return new float[]{r.exactCenterX(), r.exactCenterY()};
    }

    private static boolean contains(View group, int id) {
        if (group.getId() == id) return true;
        if (group instanceof android.view.ViewGroup) {
            final android.view.ViewGroup g = (android.view.ViewGroup) group;
            for (int i = 0; i < g.getChildCount(); i++) {
                if (contains(g.getChildAt(i), id)) return true;
            }
        }
        return false;
    }

    // -----------------------------------------------------------------------
    // Plumbing
    // -----------------------------------------------------------------------

    private void press(final int id) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View control = workspace.findViewById(id);
            assertNotNull("control " + resName(activity, id) + " must exist", control);
            assertTrue("control " + resName(activity, id) + " must be on screen",
                    control.isShown());
            control.performClick();
            return null;
        });
        settleLayout();
    }

    private boolean shown(int id) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View view = activity.findViewById(id);
            return view != null && view.isShown();
        });
    }

    private boolean drawerOpen() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.projectPopover().isOpen()
                        && workspace.projectPopover().isShown());
    }

    private float density() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> activity.getResources().getDisplayMetrics().density);
    }

    private String name(int id) {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> activity.getResources().getResourceEntryName(id));
    }

    /** A resource's name, for messages built ON the UI thread. */
    private static String resName(android.app.Activity activity, int id) {
        return activity.getResources().getResourceEntryName(id);
    }

    private String string(int id) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> activity.getString(id));
    }

    private void setCamera(float yaw, float pitch, float distance) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(NativeViewport.debugSetCameraPose(yaw, pitch, distance));
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
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

    private static double selectedArea() {
        double sum = 0.0;
        final double[] info = new double[NativeViewport.SKETCH_REGION_INFO_SIZE];
        for (long handle : selected()) {
            assertTrue(NativeViewport.sketchProfileInfo(handle, info));
            sum += info[NativeViewport.SKETCH_REGION_AREA];
        }
        return sum;
    }

    private static int candidateStatus() {
        final double[] tool = new double[NativeViewport.CAD_EXTRUDE_SIZE];
        NativeViewport.cadExtrudeToolState(tool);
        return (int) tool[NativeViewport.CAD_EXTRUDE_CANDIDATE_STATUS];
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

    // -----------------------------------------------------------------------
    // Debug attribution tokens, read back from this process's own log
    // -----------------------------------------------------------------------

    private String mark() {
        final String marker = "OSS_MARK_" + System.nanoTime() + "_" + (marks++);
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
                            || trimmed.startsWith("FORGESHAPE_SUPPORT_CHOOSER")) {
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
        Log.i(TAG, "OSS_DEVICE " + line);
    }

    private void writeFacts() {
        final File file = new File(outDir, "facts-" + System.nanoTime() + ".txt");
        try (PrintWriter out = new PrintWriter(file, "UTF-8")) {
            for (String line : facts) {
                out.println(line);
            }
        } catch (IOException error) {
            Log.w(TAG, "OSS_DEVICE facts not written: " + error);
        }
    }
}
