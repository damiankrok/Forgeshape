package com.forgeshape.app;

import static com.forgeshape.app.SketchTestSupport.dragSketch;
import static com.forgeshape.app.SketchTestSupport.selectTool;
import static com.forgeshape.app.SketchTestSupport.sketchState;
import static com.forgeshape.app.SketchTestSupport.tapSketch;
import static com.forgeshape.app.SketchTestSupport.tapWorld;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.SystemClock;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.ImageView;
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
import java.util.Locale;

/**
 * `CAD-VERTICAL-SLICE-R1` on the device: the owner's findings, fixed, driven
 * the way a user drives them — semantic view ids for every control and native
 * projections for every viewport pixel — and asserted by GEOMETRY (volume,
 * shells, bounds read from the regenerated solid), never by a picture alone.
 *
 * <p>Seven cases, one per acceptance journey:
 * <ol>
 *   <li>a rectangle around a centred circle offers TWO regions, chooses
 *       neither, and a tap extrudes the rectangle-with-a-hole;</li>
 *   <li>the canvas HUD is compact and icon-first, with 48 dp hit targets and
 *       the value on the shaft;</li>
 *   <li>a sketch on a body's face ADDS to that same body;</li>
 *   <li>and CUTS it;</li>
 *   <li>refusals are named and change nothing;</li>
 *   <li>a later feature re-edits in place, undoes, and survives save/load;</li>
 *   <li>Ready withdraws the drawing chrome.</li>
 * </ol>
 *
 * <p>Every measured value is logged as {@code CADVS_AFTER} and written with
 * the captures under {@code files/evidence/cad-vertical-slice-r1-after/},
 * which CI DEVICE pulls into its evidence.
 */
@RunWith(AndroidJUnit4.class)
public final class CadVerticalSliceTest {

    private static final String TAG = "ForgeShape";
    /** Relative volume tolerance: the kernel works in double, the mesh in float. */
    private static final double VOLUME_TOLERANCE = 1e-4;
    /** Slots of {@code cadBodyMeasure}: min X, Y, Z from MIN_X, then max X, Y, Z. */
    private static final int MEASURE_MAX_Y = NativeViewport.CAD_MEASURE_MAX_X + 1;

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private byte[] baselineProject;
    private File outDir;
    private final List<String> facts = new ArrayList<>();

    @Before
    public void setUp() {
        outDir = new File(InstrumentationRegistry.getInstrumentation().getTargetContext()
                .getExternalFilesDir(null), "evidence/cad-vertical-slice-r1-after");
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
    // 1. The owner's rectangle around a centred circle
    // =======================================================================

    @Test
    public void owner_rectangle_circle_region() {
        final int bodiesBefore = NativeViewport.sceneBodyCount();
        beginSketch(R.id.sketch_plane_xy);
        drawRectangle(4.0, 3.0);
        // The drag snaps to the view-adaptive grid, so the rectangle is whatever
        // native made of it (4 x 2.8 on the 0.2 m grid of a 2400 px phone); it
        // is read back rather than assumed. The new entity is the selected one.
        final double[] drawn = new double[NativeViewport.SKETCH_ENTITY_SIZE];
        assertTrue(NativeViewport.sketchSelectedEntity(drawn));
        assertEquals(NativeViewport.SKETCH_ENTITY_KIND_RECTANGLE,
                (int) drawn[NativeViewport.SKETCH_ENTITY_KIND]);
        final double rectWidth = drawn[NativeViewport.SKETCH_ENTITY_VALUES + 2];
        final double rectArea = rectWidth * drawn[NativeViewport.SKETCH_ENTITY_VALUES + 3];
        fact("rectangle_m", rectWidth + " x " + drawn[NativeViewport.SKETCH_ENTITY_VALUES + 3]);
        selectTool(rule.getScenario(), R.id.tool_rail_circle);
        dragSketch(rule.getScenario(), 0.0, 0.0, 0.8, 0.0);
        assertEquals("a rectangle and a circle", 2.0,
                sketchStateArray()[NativeViewport.SKETCH_ENTITY_COUNT], 0.0);
        finishSketch();
        capture("01_rectangle_circle_two_regions");

        // Two REGIONS, and neither is chosen for the user.
        final double[] ready = sketchStateArray();
        fact("regions", ready[NativeViewport.SKETCH_PROFILE_COUNT]);
        assertEquals("the rectangle-with-a-hole and the disk", 2.0,
                ready[NativeViewport.SKETCH_PROFILE_COUNT], 0.0);
        final double[] tool = toolState();
        assertEquals("nothing is auto-selected", 0.0,
                tool[NativeViewport.CAD_EXTRUDE_SELECTED_REGIONS], 0.0);
        assertEquals("so there is no arrow yet", 0.0, tool[NativeViewport.CAD_EXTRUDE_ACTIVE], 0.0);

        final long[] anchors = regionAnchors();
        assertEquals(2, anchors.length);
        long ring = NativeViewport.NO_OBJECT;
        long disk = NativeViewport.NO_OBJECT;
        final double[] info = new double[NativeViewport.SKETCH_REGION_INFO_SIZE];
        for (long anchor : anchors) {
            assertTrue(NativeViewport.sketchProfileInfo(anchor, info));
            if (info[NativeViewport.SKETCH_REGION_HOLES] == 1.0) {
                ring = anchor;
                assertEquals("the ring's outer loop is the rectangle",
                        NativeViewport.SKETCH_PROFILE_KIND_RECTANGLE,
                        (int) info[NativeViewport.SKETCH_REGION_KIND]);
            } else {
                disk = anchor;
                assertEquals("the other region is the disk",
                        NativeViewport.SKETCH_PROFILE_KIND_CIRCLE,
                        (int) info[NativeViewport.SKETCH_REGION_KIND]);
            }
            assertEquals("unselected", 0.0, info[NativeViewport.SKETCH_REGION_SELECTED], 0.0);
        }
        assertTrue("one region has the circle as its hole", ring != NativeViewport.NO_OBJECT);
        assertTrue("and one is the disk", disk != NativeViewport.NO_OBJECT);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertFalse("Extrude is absent until a region is chosen: it could not succeed",
                    workspace.findViewById(R.id.extrude_sketch).isShown());
            return null;
        });
        // Extrude with nothing chosen is refused by name and creates nothing.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.extrude_sketch).performClick();
            return null;
        });
        settleLayout();
        assertEquals("no body without a region", bodiesBefore, NativeViewport.sceneBodyCount());
        assertEquals("and the session is still Ready", NativeViewport.SKETCH_READY, sketchState());

        // A TAP between the rectangle and the circle selects the ring.
        tapSketch(rule.getScenario(), 1.5, 0.0);
        assertTrue(NativeViewport.sketchProfileInfo(ring, info));
        assertEquals("the tapped region is selected", 1.0,
                info[NativeViewport.SKETCH_REGION_SELECTED], 0.0);
        final double ringArea = info[NativeViewport.SKETCH_REGION_AREA];
        fact("ring_area_m2", ringArea);
        assertTrue(NativeViewport.sketchProfileInfo(disk, info));
        assertEquals("the disk is not", 0.0, info[NativeViewport.SKETCH_REGION_SELECTED], 0.0);
        final double diskArea = info[NativeViewport.SKETCH_REGION_AREA];
        assertEquals("the ring is the rectangle minus the disk", rectArea - diskArea, ringArea,
                1e-6);
        assertEquals("one region in the extrusion", 1.0,
                toolState()[NativeViewport.CAD_EXTRUDE_SELECTED_REGIONS], 0.0);
        assertEquals("the arrow is up", 1.0, toolState()[NativeViewport.CAD_EXTRUDE_ACTIVE], 0.0);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("and Extrude is drawn now that it can succeed",
                    workspace.findViewById(R.id.extrude_sketch).isShown());
            return null;
        });
        // The PREVIEW is the ring: the candidate the renderer draws measures
        // as the rectangle minus the disk, one shell -- a real hole, not a
        // filled outer loop.
        final double depth = toolState()[NativeViewport.CAD_EXTRUDE_DEPTH];
        final double[] ringPreview = candidate();
        fact("ring_preview_volume_m3", ringPreview[NativeViewport.CANDIDATE_MEASURE_VOLUME]);
        fact("perf.ring_preview_us", toolState()[NativeViewport.CAD_EXTRUDE_PREVIEW_MICROS]);
        assertEquals("the preview is valid", NativeViewport.CAD_OK,
                (int) ringPreview[NativeViewport.CANDIDATE_MEASURE_STATUS]);
        assertEquals("the preview has the hole", ringArea * depth,
                ringPreview[NativeViewport.CANDIDATE_MEASURE_VOLUME],
                ringArea * depth * VOLUME_TOLERANCE);
        assertEquals("in one shell", 1.0,
                ringPreview[NativeViewport.CANDIDATE_MEASURE_COMPONENTS], 0.0);
        capture("02_ring_selected_hatched");

        // Switch to the DISK: the ring is dropped (they share the circle) and
        // the preview becomes the cylinder.
        tapSketch(rule.getScenario(), 0.0, 0.0);
        assertTrue(NativeViewport.sketchProfileInfo(disk, info));
        assertEquals("the disk is selected", 1.0, info[NativeViewport.SKETCH_REGION_SELECTED], 0.0);
        assertTrue(NativeViewport.sketchProfileInfo(ring, info));
        assertEquals("and the ring was dropped", 0.0, info[NativeViewport.SKETCH_REGION_SELECTED], 0.0);
        final double[] diskPreview = candidate();
        assertTrue("a new candidate revision", diskPreview[NativeViewport.CANDIDATE_MEASURE_REVISION]
                != ringPreview[NativeViewport.CANDIDATE_MEASURE_REVISION]);
        assertEquals("the preview is the cylinder", diskArea * depth,
                diskPreview[NativeViewport.CANDIDATE_MEASURE_VOLUME],
                diskArea * depth * VOLUME_TOLERANCE);
        capture("02b_disk_selected");

        // And back to the ring.
        tapSketch(rule.getScenario(), 1.5, 0.0);
        assertTrue(NativeViewport.sketchProfileInfo(ring, info));
        assertEquals("the ring again", 1.0, info[NativeViewport.SKETCH_REGION_SELECTED], 0.0);
        assertEquals(ringArea * depth, candidate()[NativeViewport.CANDIDATE_MEASURE_VOLUME],
                ringArea * depth * VOLUME_TOLERANCE);

        // Tapping it again deselects it; tapping once more brings it back.
        tapSketch(rule.getScenario(), 1.5, 0.0);
        assertEquals("a second tap deselects", 0.0,
                toolState()[NativeViewport.CAD_EXTRUDE_SELECTED_REGIONS], 0.0);
        tapSketch(rule.getScenario(), 1.5, 0.0);
        assertEquals(1.0, toolState()[NativeViewport.CAD_EXTRUDE_SELECTED_REGIONS], 0.0);

        // Extrude: a prism with a real through hole.
        final long body = extrudeWithDepth("0.5");
        assertTrue("a CAD body", body != NativeViewport.NO_OBJECT);
        assertEquals(bodiesBefore + 1, NativeViewport.sceneBodyCount());
        final double[] m = measure(body);
        fact("ring_body_volume_m3", m[NativeViewport.CAD_MEASURE_VOLUME]);
        fact("ring_body_triangles", m[NativeViewport.CAD_MEASURE_TRIANGLES]);
        assertEquals("volume = ring area x depth", ringArea * 0.5,
                m[NativeViewport.CAD_MEASURE_VOLUME], ringArea * 0.5 * VOLUME_TOLERANCE);
        assertEquals("one watertight shell", 1.0, m[NativeViewport.CAD_MEASURE_COMPONENTS], 0.0);
        assertEquals("the full rectangle's X extent", rectWidth,
                m[NativeViewport.CAD_MEASURE_MAX_X] - m[NativeViewport.CAD_MEASURE_MIN_X], 1e-4);
        capture("03_ring_extruded");
    }

    // =======================================================================
    // 2. The compact, icon-first extrude HUD
    // =======================================================================

    @Test
    public void compact_extrude_hud() {
        beginSketch(R.id.sketch_plane_xy);
        drawRectangle(2.0, 1.0);
        finishSketch();
        assertEquals("one region, selected", 1.0,
                toolState()[NativeViewport.CAD_EXTRUDE_SELECTED_REGIONS], 0.0);
        capture("04_compact_hud_one_side");

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final float density = activity.getResources().getDisplayMetrics().density;
            final View canvas = workspace.cadExtrudeCanvas();
            final View cluster = canvas.findViewById(R.id.cad_extrude_cluster);
            assertNotNull(cluster);
            assertTrue("the cluster is on screen", cluster.isShown());
            final Rect box = screenRect(cluster);
            final Rect viewport = screenRect(workspace.findViewById(R.id.viewport_surface));
            fact("hud.cluster", box.toShortString() + " w_dp=" + box.width() / density
                    + " h_dp=" + box.height() / density + " viewport_pct="
                    + pct(box, viewport));
            // The pre-change cluster was 1027 x 171 px (6.78 % of the viewport).
            assertTrue("the cluster stays compact: " + box.width() / density + " dp",
                    box.width() / density <= 360f);
            assertTrue("well under the 6.78 % it covered before: " + pct(box, viewport),
                    pct(box, viewport) < 3.0);

            // Icons, not text pills, while Tool Labels is off (the default).
            for (int id : new int[]{R.id.cad_extrude_extent, R.id.cad_extrude_operation,
                    R.id.cad_extrude_flip}) {
                final View control = canvas.findViewById(id);
                assertNotNull("control " + id, control);
                assertTrue("shown " + id, control.isShown());
                assertIconControl(control, density, "hud." + name(activity, id));
            }

            // The exact value stands ON the arrow shaft's midpoint.
            final double[] tool = new double[NativeViewport.CAD_EXTRUDE_SIZE];
            NativeViewport.cadExtrudeToolState(tool);
            final View value = canvas.findViewById(R.id.cad_extrude_depth_value);
            assertTrue(value.isShown());
            final Rect v = screenRect(value);
            final double dx = v.exactCenterX() - (viewport.left + tool[NativeViewport.CAD_EXTRUDE_LABEL_X]);
            final double dy = v.exactCenterY() - (viewport.top + tool[NativeViewport.CAD_EXTRUDE_LABEL_Y]);
            final double offDp = Math.hypot(dx, dy) / density;
            fact("hud.value_to_shaft_dp", offDp);
            assertTrue("the value is on the shaft (was 99.5 dp away): " + offDp, offDp <= 4.0);
            assertTrue("value hit height >= 48 dp", value.getHeight() >= Math.round(48f * density) - 1);
            return null;
        });

        // An orbit moves the arrow; the value follows its anchor on the next
        // refresh rather than standing where it was.
        final double[] beforeOrbit = toolState();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(NativeViewport.debugSetCameraPose(1.1f, 0.5f, 9.0f));
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        final double[] afterOrbit = toolState();
        assertTrue("the orbit moved the anchor",
                Math.hypot(afterOrbit[NativeViewport.CAD_EXTRUDE_LABEL_X]
                                - beforeOrbit[NativeViewport.CAD_EXTRUDE_LABEL_X],
                        afterOrbit[NativeViewport.CAD_EXTRUDE_LABEL_Y]
                                - beforeOrbit[NativeViewport.CAD_EXTRUDE_LABEL_Y]) > 4.0);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final float density = activity.getResources().getDisplayMetrics().density;
            final Rect viewport = screenRect(workspace.findViewById(R.id.viewport_surface));
            final Rect v = screenRect(workspace.cadExtrudeCanvas()
                    .findViewById(R.id.cad_extrude_depth_value));
            final double off = Math.hypot(
                    v.exactCenterX() - (viewport.left + afterOrbit[NativeViewport.CAD_EXTRUDE_LABEL_X]),
                    v.exactCenterY() - (viewport.top + afterOrbit[NativeViewport.CAD_EXTRUDE_LABEL_Y]))
                    / density;
            fact("hud.value_to_shaft_after_orbit_dp", off);
            assertTrue("the value followed the orbit: " + off, off <= 4.0);
            return null;
        });

        // The extent palette opens from the one extent control and closes on a choice.
        clickCanvas(R.id.cad_extrude_extent);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View canvas = workspace.cadExtrudeCanvas();
            assertTrue("the extent palette is open",
                    canvas.findViewById(R.id.cad_extrude_extent_palette).isShown());
            final float density = activity.getResources().getDisplayMetrics().density;
            for (int id : new int[]{R.id.cad_extrude_extent_one_side,
                    R.id.cad_extrude_extent_symmetric, R.id.cad_extrude_extent_two_sides}) {
                assertIconControl(canvas.findViewById(id), density, "hud." + name(activity, id));
            }
            return null;
        });
        capture("05_compact_hud_extent_palette");
        clickCanvas(R.id.cad_extrude_extent_symmetric);
        assertEquals(NativeViewport.EXTENT_SYMMETRIC,
                (int) toolState()[NativeViewport.CAD_EXTRUDE_EXTENT]);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View canvas = workspace.cadExtrudeCanvas();
            assertFalse("the palette closed on the choice",
                    canvas.findViewById(R.id.cad_extrude_extent_palette).isShown());
            assertFalse("Flip is absent outside One Side",
                    canvas.findViewById(R.id.cad_extrude_flip).isShown());
            return null;
        });
        capture("06_compact_hud_symmetric");

        // Tool Labels ON adds short captions without turning the row into pills.
        final AppPreferences before = onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.currentPreferences());
        assertFalse("Tool Labels defaults to OFF", before.toolLabels());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onToolLabelsChosen(true);
            return null;
        });
        settleLayout();
        try {
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                final float density = activity.getResources().getDisplayMetrics().density;
                final View cluster = workspace.cadExtrudeCanvas().findViewById(R.id.cad_extrude_cluster);
                final Rect box = screenRect(cluster);
                fact("hud.cluster_labels_on", box.toShortString() + " w_dp=" + box.width() / density);
                assertTrue("labels on stays compact: " + box.width() / density,
                        box.width() / density <= 360f);
                assertTrue("a caption is drawn",
                        containsShownText(cluster, activity.getString(R.string.extent_symmetric))
                                || containsShownTextIgnoreCase(cluster, "symmetric"));
                return null;
            });
            capture("07_compact_hud_labels_on");

            // The preference is application state: it survives the Activity
            // being rebuilt, and it never touched the project.
            final byte[] project = NativeViewport.encodeProject();
            rule.getScenario().recreate();
            settleLayout();
            assertTrue("Tool Labels survived the Activity being recreated",
                    onWorkspace(rule.getScenario(),
                            (activity, workspace) -> workspace.currentPreferences().toolLabels()));
            assertArrayEquals("and wrote no project byte", project, NativeViewport.encodeProject());
        } finally {
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                workspace.onToolLabelsChosen(false);
                return null;
            });
            settleLayout();
        }
    }

    // =======================================================================
    // 3. Add lands in the SAME body
    // =======================================================================

    @Test
    public void new_body_then_add_same_body() {
        final long base = baseBodyOnXz();
        final int bodies = NativeViewport.sceneBodyCount();
        final double baseVolume = measure(base)[NativeViewport.CAD_MEASURE_VOLUME];
        assertEquals("a 2 x 2 x 1 base", 4.0, baseVolume, 4.0 * VOLUME_TOLERANCE);

        final double area = faceSketchSquare(0.8);
        final double[] tool = toolState();
        final int offered = (int) tool[NativeViewport.CAD_EXTRUDE_OPERATIONS_AVAILABLE];
        fact("face_sketch.operations_available", offered);
        assertTrue("Add is offered on a face", (offered & NativeViewport.OPERATION_BIT_ADD) != 0);
        assertTrue("and Cut", (offered & NativeViewport.OPERATION_BIT_CUT) != 0);
        assertEquals("New Body is still the default", NativeViewport.OPERATION_NEW_BODY,
                (int) tool[NativeViewport.CAD_EXTRUDE_OPERATION]);

        chooseOperationOnCanvas(R.id.cad_extrude_operation_add);
        assertEquals(NativeViewport.OPERATION_ADD,
                (int) toolState()[NativeViewport.CAD_EXTRUDE_OPERATION]);
        assertEquals("the candidate is valid", NativeViewport.CAD_OK,
                (int) toolState()[NativeViewport.CAD_EXTRUDE_CANDIDATE_STATUS]);
        assertEquals("the preview names the base as its target", base,
                (long) toolState()[NativeViewport.CAD_EXTRUDE_TARGET_BODY]);
        fact("perf.add_preview_us", toolState()[NativeViewport.CAD_EXTRUDE_PREVIEW_MICROS]);
        // A drag of the arrow re-evaluates the Add latest-only: the preview is
        // valid after it and its time is what a drag sample costs.
        dragArrowAlongAxis("add");
        capture("08_add_preview");

        final int undo = NativeViewport.constructionUndoDepth();
        final long result = extrudeWithDepth("0.5");
        assertEquals("Add returns the SAME body", base, result);
        assertEquals("no body was created", bodies, NativeViewport.sceneBodyCount());
        assertEquals("the base is still active", base, NativeViewport.sceneActiveBodyId());
        assertEquals("one history step", undo + 1, NativeViewport.constructionUndoDepth());
        assertEquals("two features", 2, NativeViewport.cadFeatureCount(base));
        final double[] m = measure(base);
        fact("add.volume_m3", m[NativeViewport.CAD_MEASURE_VOLUME]);
        assertEquals("volume grew by the boss", 4.0 + area * 0.5,
                m[NativeViewport.CAD_MEASURE_VOLUME], 4.5 * VOLUME_TOLERANCE);
        assertEquals("still one shell", 1.0, m[NativeViewport.CAD_MEASURE_COMPONENTS], 0.0);
        assertEquals("the boss rises above the cap", 1.5, m[MEASURE_MAX_Y], 1e-4);
        capture("09_add_committed");

        // One Undo removes exactly the Add; Redo restores it.
        undo();
        assertEquals(1, NativeViewport.cadFeatureCount(base));
        assertEquals(4.0, measure(base)[NativeViewport.CAD_MEASURE_VOLUME], 4.0 * VOLUME_TOLERANCE);
        assertEquals(bodies, NativeViewport.sceneBodyCount());
        redo();
        assertEquals(2, NativeViewport.cadFeatureCount(base));
        assertEquals(4.0 + area * 0.5, measure(base)[NativeViewport.CAD_MEASURE_VOLUME],
                4.5 * VOLUME_TOLERANCE);
    }

    // =======================================================================
    // 4. Cut removes material from the SAME body
    // =======================================================================

    @Test
    public void same_body_cut() {
        final long base = baseBodyOnXz();
        final int bodies = NativeViewport.sceneBodyCount();
        final double area = faceSketchSquare(0.8);
        chooseOperationOnCanvas(R.id.cad_extrude_operation_cut);
        final double[] tool = toolState();
        assertEquals(NativeViewport.OPERATION_CUT, (int) tool[NativeViewport.CAD_EXTRUDE_OPERATION]);
        assertEquals("a Cut on a face grows INTO the body", NativeViewport.EXTRUDE_AGAINST_NORMAL,
                (int) tool[NativeViewport.CAD_EXTRUDE_DIRECTION]);
        assertEquals("the candidate is valid", NativeViewport.CAD_OK,
                (int) tool[NativeViewport.CAD_EXTRUDE_CANDIDATE_STATUS]);
        fact("perf.cut_preview_us", toolState()[NativeViewport.CAD_EXTRUDE_PREVIEW_MICROS]);
        dragArrowAlongAxis("cut");
        capture("10_cut_preview");

        final int undo = NativeViewport.constructionUndoDepth();
        final long result = extrudeWithDepth("0.5");
        assertEquals("Cut returns the SAME body", base, result);
        assertEquals("no body was created", bodies, NativeViewport.sceneBodyCount());
        assertEquals("one history step", undo + 1, NativeViewport.constructionUndoDepth());
        final double[] m = measure(base);
        fact("cut.volume_m3", m[NativeViewport.CAD_MEASURE_VOLUME]);
        assertEquals("volume lost the pocket", 4.0 - area * 0.5,
                m[NativeViewport.CAD_MEASURE_VOLUME], 4.0 * VOLUME_TOLERANCE);
        assertEquals("one shell", 1.0, m[NativeViewport.CAD_MEASURE_COMPONENTS], 0.0);
        assertEquals("the bounds are unchanged by a pocket", 1.0, m[MEASURE_MAX_Y], 1e-4);
        capture("11_cut_committed");

        undo();
        assertEquals(4.0, measure(base)[NativeViewport.CAD_MEASURE_VOLUME], 4.0 * VOLUME_TOLERANCE);
        redo();
        assertEquals(4.0 - area * 0.5, measure(base)[NativeViewport.CAD_MEASURE_VOLUME],
                4.0 * VOLUME_TOLERANCE);
    }

    // =======================================================================
    // 5. Refusals are named, previewed and change nothing
    // =======================================================================

    @Test
    public void operation_refusal() {
        // A world-plane sketch has no body to add to or cut.
        beginSketch(R.id.sketch_plane_xy);
        drawRectangle(1.0, 1.0);
        finishSketch();
        final double[] world = toolState();
        assertEquals("only New Body on a world plane", NativeViewport.OPERATION_BIT_NEW_BODY,
                (int) world[NativeViewport.CAD_EXTRUDE_OPERATIONS_AVAILABLE]);
        final int refusedAdd = onWorkspace(rule.getScenario(), (activity, workspace) ->
                NativeViewport.sketchSetOperation(NativeViewport.OPERATION_ADD));
        assertEquals("Add is refused by name", NativeViewport.CAD_OPERATION_NEEDS_TARGET,
                refusedAdd);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View canvas = workspace.cadExtrudeCanvas();
            final View add = canvas.findViewById(R.id.cad_extrude_operation_add);
            final View cut = canvas.findViewById(R.id.cad_extrude_operation_cut);
            assertTrue("no Add control is drawn", add == null || !add.isShown());
            assertTrue("no Cut control is drawn", cut == null || !cut.isShown());
            return null;
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onCancelSketchRequested();
            return null;
        });
        settleLayout();

        final long base = baseBodyOnXz();
        faceSketchSquare(0.8);
        final byte[] before = NativeViewport.encodeProject();
        final int bodies = NativeViewport.sceneBodyCount();
        final int undo = NativeViewport.constructionUndoDepth();

        // A Cut pointed OUT of the body misses it: previewed, then refused.
        chooseOperationOnCanvas(R.id.cad_extrude_operation_cut);
        clickCanvas(R.id.cad_extrude_flip);
        assertEquals("the preview names the refusal", NativeViewport.CAD_CUT_NO_INTERSECTION,
                (int) toolState()[NativeViewport.CAD_EXTRUDE_CANDIDATE_STATUS]);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertFalse("Extrude is absent while the preview is invalid",
                    workspace.findViewById(R.id.extrude_sketch).isShown());
            // The reason is named where the preview is: on the operation badge.
            final CharSequence why = workspace.cadExtrudeCanvas()
                    .findViewById(R.id.cad_extrude_operation).getContentDescription();
            fact("refusal.badge_description", why);
            assertTrue("and the badge says why: " + why, why != null && why.toString().contains(
                    CadStatusMessages.describe(activity, NativeViewport.CAD_CUT_NO_INTERSECTION)));
            return null;
        });
        capture("12_cut_misses_preview");
        final long refused = extrudeWithDepth("0.5");
        assertEquals("nothing committed", NativeViewport.NO_OBJECT, refused);
        assertEquals("still Ready", NativeViewport.SKETCH_READY, sketchState());
        assertEquals(bodies, NativeViewport.sceneBodyCount());
        assertEquals(undo, NativeViewport.constructionUndoDepth());
        assertEquals(1, NativeViewport.cadFeatureCount(base));

        // An Add outside the face's footprint would be a separate piece.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onCancelSketchRequested();
            return null;
        });
        settleLayout();
        assertArrayEquals("cancelling the refused Cut changed nothing", before,
                NativeViewport.encodeProject());
        faceSketch(1.4, 1.4, 1.8, 1.8);
        chooseOperationOnCanvas(R.id.cad_extrude_operation_add);
        assertEquals("a disjoint Add is refused by name", NativeViewport.CAD_ADD_DISJOINT,
                (int) toolState()[NativeViewport.CAD_EXTRUDE_CANDIDATE_STATUS]);
        assertEquals(NativeViewport.NO_OBJECT, extrudeWithDepth("0.5"));
        assertEquals(bodies, NativeViewport.sceneBodyCount());
        assertEquals(undo, NativeViewport.constructionUndoDepth());

        // Leaving the sketch changes the project by nothing at all.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onCancelSketchRequested();
            return null;
        });
        settleLayout();
        assertArrayEquals("a refused Add/Cut changed no project byte", before,
                NativeViewport.encodeProject());
    }

    // =======================================================================
    // 6. A later feature re-edits in place, undoes, and survives save/load
    // =======================================================================

    @Test
    public void feature_edit_roundtrip() {
        final long base = baseBodyOnXz();
        final double area = faceSketchSquare(0.8);
        chooseOperationOnCanvas(R.id.cad_extrude_operation_add);
        assertEquals(base, extrudeWithDepth("0.5"));
        final int bodies = NativeViewport.sceneBodyCount();
        assertEquals(2, NativeViewport.cadFeatureCount(base));

        // The feature list names both features; the second reopens the Add.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onNativeStateChanged();
            WorkspaceTestSupport.openPrecision(workspace);
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final ViewGroup list = workspace.findViewById(R.id.cad_feature_list);
            assertNotNull(list);
            assertTrue("the feature list is shown", list.isShown());
            final List<View> rows = new ArrayList<>();
            collectById(list, R.id.cad_feature_row, rows);
            fact("feature_list.rows", rows.size());
            assertEquals("two feature rows", 2, rows.size());
            View second = null;
            for (View row : rows) {
                if (Long.valueOf(2L).equals(row.getTag())) {
                    second = row;
                }
            }
            assertNotNull("feature 2 has a row", second);
            second.performClick();
            return null;
        });
        settleLayout();
        assertEquals("editing the Add in place", base, NativeViewport.sketchEditingBodyId());
        assertEquals(2L, NativeViewport.sketchEditingFeatureId());
        assertEquals("it opens on its extrusion", NativeViewport.SKETCH_READY, sketchState());
        assertEquals(NativeViewport.OPERATION_ADD,
                (int) toolState()[NativeViewport.CAD_EXTRUDE_OPERATION]);
        capture("13_feature_edit_open");

        final int undo = NativeViewport.constructionUndoDepth();
        assertEquals("Finish returns the same body", base, extrudeWithDepth("0.25"));
        assertEquals(bodies, NativeViewport.sceneBodyCount());
        assertEquals("one step", undo + 1, NativeViewport.constructionUndoDepth());
        assertEquals(2, NativeViewport.cadFeatureCount(base));
        final double edited = 4.0 + area * 0.25;
        assertEquals(edited, measure(base)[NativeViewport.CAD_MEASURE_VOLUME], 4.5 * VOLUME_TOLERANCE);
        undo();
        assertEquals(4.0 + area * 0.5, measure(base)[NativeViewport.CAD_MEASURE_VOLUME],
                4.5 * VOLUME_TOLERANCE);
        redo();
        assertEquals(edited, measure(base)[NativeViewport.CAD_MEASURE_VOLUME], 4.5 * VOLUME_TOLERANCE);

        // An UPSTREAM edit: the base grows from 1 m to 1.5 m through the CAD
        // precision surface, and the Add standing on its far cap regenerates
        // with it -- carried to the new cap, one step, the same body.
        final int undoUpstream = NativeViewport.constructionUndoDepth();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onNativeStateChanged();
            WorkspaceTestSupport.openPrecision(workspace);
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final EditText depthField = workspace.findViewById(R.id.field_cad_depth);
            assertNotNull("the base's depth is editable", depthField);
            depthField.setText("1.5");
            workspace.findViewById(R.id.apply_cad).performClick();
            return null;
        });
        settleLayout();
        assertEquals("one step", undoUpstream + 1, NativeViewport.constructionUndoDepth());
        assertEquals(bodies, NativeViewport.sceneBodyCount());
        final double[] upstream = measure(base);
        fact("upstream_edit.volume_m3", upstream[NativeViewport.CAD_MEASURE_VOLUME]);
        assertEquals("the base and the carried Add", 6.0 + area * 0.25,
                upstream[NativeViewport.CAD_MEASURE_VOLUME], 6.5 * VOLUME_TOLERANCE);
        assertEquals("the Add now rises from the new cap", 1.75, upstream[MEASURE_MAX_Y], 1e-4);
        capture("13b_upstream_edit_carries_the_add");

        // Save and reopen: CADB v5 carries the chain; the solid is identical.
        final double[] beforeSave = measure(base);
        final byte[] saved = NativeViewport.encodeProject();
        fact("feature_chain_project_bytes", saved.length);
        final int loaded = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int status = NativeViewport.loadProject(saved);
            workspace.onNativeStateChanged();
            return status;
        });
        assertEquals("the project reopens", NativeViewport.PROJECT_OK, loaded);
        settleLayout();
        assertEquals("the body keeps its id", 2, NativeViewport.cadFeatureCount(base));
        final double[] afterLoad = measure(base);
        assertEquals("the same volume", beforeSave[NativeViewport.CAD_MEASURE_VOLUME],
                afterLoad[NativeViewport.CAD_MEASURE_VOLUME], 0.0);
        assertEquals("the same triangles", beforeSave[NativeViewport.CAD_MEASURE_TRIANGLES],
                afterLoad[NativeViewport.CAD_MEASURE_TRIANGLES], 0.0);
        assertArrayEquals("and re-encodes byte-identically", saved, NativeViewport.encodeProject());
        capture("14_feature_chain_reopened");
    }

    // =======================================================================
    // 7. Ready withdraws the drawing chrome
    // =======================================================================

    @Test
    public void ui_context_withdrawal() {
        beginSketch(R.id.sketch_plane_xy);
        drawRectangle(2.0, 1.0);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("drawing: the navigator is up",
                    workspace.findViewById(R.id.sketch_orientation_navigator).isShown());
            assertTrue("and the sketch rail",
                    workspace.findViewById(R.id.tool_rail_rectangle).isShown());
            return null;
        });
        finishSketch();
        capture("15_ready_chrome");
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final Rect viewport = screenRect(workspace.findViewById(R.id.viewport_surface));
            assertFalse("Ready: the orientation navigator is withdrawn",
                    workspace.findViewById(R.id.sketch_orientation_navigator).isShown());
            assertFalse("the drawing tools are withdrawn",
                    workspace.findViewById(R.id.tool_rail_rectangle).isShown());
            assertFalse("the selected-Line dimension is withdrawn",
                    workspace.findViewById(R.id.sketch_dimension_label).isShown());
            assertFalse("the precision surface is collapsed by default",
                    workspace.propertyInspector().isOpen());
            assertTrue("Back to Sketch is offered",
                    workspace.findViewById(R.id.back_to_sketch).isShown());
            assertTrue("Cancel is offered", workspace.findViewById(R.id.cancel_sketch).isShown());
            assertTrue("the precision toggle stays",
                    workspace.findViewById(R.id.precision_toggle).isShown());
            assertTrue("Extrude is the one transition",
                    workspace.findViewById(R.id.extrude_sketch).isShown());
            final View cluster = workspace.cadExtrudeCanvas().findViewById(R.id.cad_extrude_cluster);
            assertTrue("the HUD is up", cluster.isShown());
            final View host = workspace.findViewById(R.id.workspace_trailing_host);
            fact("ready.trailing_host", screenRect(host).toShortString() + " viewport_pct="
                    + pct(screenRect(host), viewport));
            fact("ready.hud", screenRect(cluster).toShortString() + " viewport_pct="
                    + pct(screenRect(cluster), viewport));
            return null;
        });

        // Back to Sketch brings the drawing chrome back, exactly.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.back_to_sketch).performClick();
            return null;
        });
        settleLayout();
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(workspace.findViewById(R.id.sketch_orientation_navigator).isShown());
            assertTrue(workspace.findViewById(R.id.tool_rail_rectangle).isShown());
            return null;
        });

        // Opening the precision surface in Ready is still one tap away.
        finishSketch();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openPrecision(workspace);
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(workspace.propertyInspector().isOpen());
            assertNotNull("the region is listed",
                    workspace.sketchEditor().findViewById(R.id.sketch_profile_option));
            return null;
        });
        capture("16_ready_precision_on_request");
    }

    // -----------------------------------------------------------------------
    // Journeys
    // -----------------------------------------------------------------------

    /** A 2 x 2 x 1 CAD body on XZ, so its far cap faces +Y at y = 1. */
    private long baseBodyOnXz() {
        final int before = NativeViewport.sceneBodyCount();
        beginSketch(R.id.sketch_plane_xz);
        drawRectangle(2.0, 2.0);
        finishSketch();
        final long base = extrudeWithDepth("1");
        assertTrue("a base body", base != NativeViewport.NO_OBJECT);
        assertEquals(before + 1, NativeViewport.sceneBodyCount());
        return base;
    }

    /**
     * Opens a sketch on the base's far cap through the spatial chooser (aim,
     * then confirm), draws a centred square and finishes. Returns its area.
     */
    private double faceSketchSquare(double side) {
        return faceSketch(-side / 2, -side / 2, side / 2, side / 2);
    }

    /** A rectangle between two corners of a sketch on the base's far cap. */
    private double faceSketch(double u0, double v0, double u1, double v1) {
        assertTrue(NativeViewport.debugSetCameraPose(0.7f, 0.9f, 8.0f));
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(NativeViewport.supportChooserBegin(true));
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        tapWorld(rule.getScenario(), 0.0, 1.0, 0.0);
        tapWorld(rule.getScenario(), 0.0, 1.0, 0.0);
        assertEquals("a face-supported sketch is open", NativeViewport.SKETCH_EDITING, sketchState());
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), u0, v0, u1, v1);
        finishSketch();
        final long[] anchors = regionAnchors();
        assertEquals("one region", 1, anchors.length);
        final double[] info = new double[NativeViewport.SKETCH_REGION_INFO_SIZE];
        assertTrue(NativeViewport.sketchProfileInfo(anchors[0], info));
        fact("face_sketch.area_m2", info[NativeViewport.SKETCH_REGION_AREA]);
        return info[NativeViewport.SKETCH_REGION_AREA];
    }

    private void chooseOperationOnCanvas(int optionId) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View canvas = workspace.cadExtrudeCanvas();
            final View badge = canvas.findViewById(R.id.cad_extrude_operation);
            assertTrue("the operation badge is shown", badge.isShown());
            badge.performClick();
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

    /**
     * Types a depth where the user types it and presses Extrude. Returns the
     * body the commit landed in (new, or the SAME one for Add, Cut and an
     * edit), or NO_OBJECT when it was refused.
     */
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

    /**
     * Drags the extrude arrow a little along its own axis, from native's own
     * projected tip, through real MotionEvents; records the preview's cost and
     * asserts the candidate is still valid and actually moved.
     */
    private void dragArrowAlongAxis(String label) {
        final double[] before = toolState();
        assertEquals("the arrow is live", 1.0, before[NativeViewport.CAD_EXTRUDE_ACTIVE], 0.0);
        final double revisionBefore = before[NativeViewport.CAD_EXTRUDE_CANDIDATE_REVISION];
        final int undoBefore = NativeViewport.constructionUndoDepth();
        final float tipX = (float) before[NativeViewport.CAD_EXTRUDE_TIP_X];
        final float tipY = (float) before[NativeViewport.CAD_EXTRUDE_TIP_Y];
        final float labelX = (float) before[NativeViewport.CAD_EXTRUDE_LABEL_X];
        final float labelY = (float) before[NativeViewport.CAD_EXTRUDE_LABEL_Y];
        // (tip - label) is half the shaft on screen; a quarter of it more is a
        // modest growth that stays on screen.
        final float toX = tipX + (tipX - labelX) * 0.5f;
        final float toY = tipY + (tipY - labelY) * 0.5f;
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final long down = SystemClock.uptimeMillis();
            sendTouch(viewport, down, down, MotionEvent.ACTION_DOWN, tipX, tipY);
            for (int step = 1; step <= 8; ++step) {
                final float t = step / 8f;
                sendTouch(viewport, down, down + step * 12L, MotionEvent.ACTION_MOVE,
                        tipX + (toX - tipX) * t, tipY + (toY - tipY) * t);
            }
            sendTouch(viewport, down, down + 128L, MotionEvent.ACTION_UP, toX, toY);
            return null;
        });
        settleLayout();
        final double[] after = toolState();
        fact("perf." + label + "_drag_preview_us", after[NativeViewport.CAD_EXTRUDE_PREVIEW_MICROS]);
        fact(label + ".drag_depth_m", before[NativeViewport.CAD_EXTRUDE_DEPTH] + " -> "
                + after[NativeViewport.CAD_EXTRUDE_DEPTH]);
        assertTrue("the drag changed the depth",
                Math.abs(after[NativeViewport.CAD_EXTRUDE_DEPTH]
                        - before[NativeViewport.CAD_EXTRUDE_DEPTH]) > 1e-3);
        assertTrue("a new candidate revision",
                after[NativeViewport.CAD_EXTRUDE_CANDIDATE_REVISION] != revisionBefore);
        assertEquals("and the dragged candidate is still valid", NativeViewport.CAD_OK,
                (int) after[NativeViewport.CAD_EXTRUDE_CANDIDATE_STATUS]);
        assertEquals("a drag of a staged extrusion records nothing", undoBefore,
                NativeViewport.constructionUndoDepth());
    }

    private static void sendTouch(View target, long downTime, long eventTime, int action, float x,
                                  float y) {
        final MotionEvent event = MotionEvent.obtain(downTime, eventTime, action, x, y, 0);
        try {
            target.dispatchTouchEvent(event);
        } finally {
            event.recycle();
        }
    }

    private void clickCanvas(final int id) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.cadExtrudeCanvas().findViewById(id).performClick();
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

    private void redo() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.constructionRedo();
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    // -----------------------------------------------------------------------
    // Reading native truth
    // -----------------------------------------------------------------------

    private static double[] sketchStateArray() {
        final double[] state = new double[NativeViewport.SKETCH_STATE_SIZE];
        NativeViewport.sketchState(state);
        return state;
    }

    private static double[] toolState() {
        final double[] tool = new double[NativeViewport.CAD_EXTRUDE_SIZE];
        NativeViewport.cadExtrudeToolState(tool);
        return tool;
    }

    private static long[] regionAnchors() {
        final int count = NativeViewport.sketchProfiles(null);
        final long[] anchors = new long[Math.max(count, 0)];
        final int written = NativeViewport.sketchProfiles(anchors);
        assertEquals(count, written);
        return anchors;
    }

    private static double[] candidate() {
        final double[] out = new double[NativeViewport.CANDIDATE_MEASURE_SIZE];
        assertTrue("a Ready sketch has a candidate", NativeViewport.sketchCandidateMeasure(out));
        return out;
    }

    private static double[] measure(long body) {
        final double[] out = new double[NativeViewport.CAD_MEASURE_SIZE];
        assertTrue("the body measures as a CAD solid", NativeViewport.cadBodyMeasure(body, out));
        return out;
    }

    // -----------------------------------------------------------------------
    // Views
    // -----------------------------------------------------------------------

    /**
     * An icon control: a 48 dp hit rectangle, a drawn glyph of 24..32 dp, and a
     * content description that says what it is.
     */
    private void assertIconControl(View control, float density, String label) {
        assertNotNull(label, control);
        final int floor = Math.round(48f * density) - 1;
        assertTrue(label + " hit width >= 48 dp: " + control.getWidth(), control.getWidth() >= floor);
        assertTrue(label + " hit height >= 48 dp: " + control.getHeight(),
                control.getHeight() >= floor);
        assertEquals(label + " is not scaled (the hit area must not shrink with the glyph)",
                1.0f, control.getScaleX(), 0.0f);
        final ImageView glyph = firstImage(control);
        assertNotNull(label + " draws an icon", glyph);
        final int glyphW = glyph.getWidth() - glyph.getPaddingLeft() - glyph.getPaddingRight();
        final int glyphH = glyph.getHeight() - glyph.getPaddingTop() - glyph.getPaddingBottom();
        final float glyphDp = Math.min(glyphW, glyphH) / density;
        fact(label, screenRect(control).toShortString() + " hit_dp="
                + control.getWidth() / density + "x" + control.getHeight() / density
                + " glyph_dp=" + glyphDp);
        assertTrue(label + " glyph 24..32 dp: " + glyphDp, glyphDp >= 23.5f && glyphDp <= 32.5f);
        final CharSequence description = control.getContentDescription();
        assertTrue(label + " has a description", description != null && description.length() > 0);
    }

    private static ImageView firstImage(View view) {
        if (view instanceof ImageView) {
            return (ImageView) view;
        }
        if (view instanceof ViewGroup) {
            final ViewGroup group = (ViewGroup) view;
            for (int i = 0; i < group.getChildCount(); ++i) {
                final ImageView found = firstImage(group.getChildAt(i));
                if (found != null) {
                    return found;
                }
            }
        }
        return null;
    }

    private static void collectById(View view, int id, List<View> out) {
        if (view.getId() == id) {
            out.add(view);
        }
        if (view instanceof ViewGroup) {
            final ViewGroup group = (ViewGroup) view;
            for (int i = 0; i < group.getChildCount(); ++i) {
                collectById(group.getChildAt(i), id, out);
            }
        }
    }

    private static boolean containsShownText(View root, String text) {
        if (root instanceof TextView && root.isShown()
                && ((TextView) root).getText().toString().equals(text)) {
            return true;
        }
        if (root instanceof ViewGroup) {
            final ViewGroup group = (ViewGroup) root;
            for (int i = 0; i < group.getChildCount(); ++i) {
                if (containsShownText(group.getChildAt(i), text)) {
                    return true;
                }
            }
        }
        return false;
    }

    private static boolean containsShownTextIgnoreCase(View root, String text) {
        if (root instanceof TextView && root.isShown()
                && ((TextView) root).getText().toString().toLowerCase(Locale.ROOT)
                        .contains(text.toLowerCase(Locale.ROOT))) {
            return true;
        }
        if (root instanceof ViewGroup) {
            final ViewGroup group = (ViewGroup) root;
            for (int i = 0; i < group.getChildCount(); ++i) {
                if (containsShownTextIgnoreCase(group.getChildAt(i), text)) {
                    return true;
                }
            }
        }
        return false;
    }

    private static String name(ForgeShapeActivity activity, int id) {
        return activity.getResources().getResourceEntryName(id);
    }

    private static Rect screenRect(View view) {
        final int[] at = new int[2];
        view.getLocationOnScreen(at);
        return new Rect(at[0], at[1], at[0] + Math.round(view.getWidth() * view.getScaleX()),
                at[1] + Math.round(view.getHeight() * view.getScaleY()));
    }

    private static double pct(Rect box, Rect viewport) {
        return 100.0 * box.width() * box.height() / ((double) viewport.width() * viewport.height());
    }

    // -----------------------------------------------------------------------
    // Evidence
    // -----------------------------------------------------------------------

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
        Log.i(TAG, "CADVS_AFTER " + line);
    }

    private void writeFacts() {
        final File file = new File(outDir, "facts-" + System.nanoTime() + ".txt");
        try (PrintWriter out = new PrintWriter(file, "UTF-8")) {
            for (String line : facts) {
                out.println(line);
            }
        } catch (IOException error) {
            Log.w(TAG, "CADVS_AFTER facts not written: " + error);
        }
    }
}
