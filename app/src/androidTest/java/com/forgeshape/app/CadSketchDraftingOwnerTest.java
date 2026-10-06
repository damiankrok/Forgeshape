package com.forgeshape.app;

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
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.SystemClock;
import android.util.Log;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
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

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * `CAD-SKETCH-DRAFTING-TOOLKIT-E2E-R1` on the device, on the owner's own path:
 * Home → New Project → CAD, geometry drawn on the canvas, then the sketch
 * actions palette under the orientation navigator — Select multiple,
 * Dimension, Make Construction / Make Regular, Trim, Extend, Offset, Mirror,
 * Delete — and the persistent dimension labels on the drawing.
 *
 * <p>Every act that lands on the DRAWING is a real window touch: a tap that
 * selects, trims, extends or picks an axis, a drag that draws or offsets, a
 * two-finger pinch that zooms. Chrome is pressed as a user presses it. Truth
 * is read from native (entities, roles, dimensions, the drafting state, the
 * candidate and body measures, the encoded project) and from the status
 * line's own text; screenshots are evidence only.
 *
 * <p>Geometry is laid out in units of {@code D}: 60 dp of the sketch view in
 * metres, measured at run time, so every snap aperture (24 dp, inner 8 dp,
 * guides 12 dp) and every hit tolerance (24 dp) is respected on any screen.
 * The scenes stand LEFT of the sketch origin, clear of the trailing column,
 * and are spaced so that no label stands on a neighbour a later tap aims at.
 *
 * <p><b>Every touch states its precondition before it is made.</b> A tap goes
 * to ONE stated point, which must project onto the viewport with no clickable
 * chrome over it (a label, the Modify column, the rail) -- there is no search
 * over candidate points. A drag needs that only of its DOWN: the view that
 * takes the Down receives the rest of the gesture, exactly as for a finger. A
 * selection a journey depends on is made by a real tap, and cleared by a real
 * tap on empty drawing; the native selection door is used only to read labels
 * and to clean up between cases.
 */
@RunWith(AndroidJUnit4.class)
public final class CadSketchDraftingOwnerTest {

    private static final String TAG = "ForgeShape";
    /** Relative tolerance of a 32-segment revolve against Pappus (sin x / x at 2π/32). */
    private static final double PAPPUS_TOLERANCE = 0.015;
    private static final double EXACT = 1e-9;

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private File outDir;
    private final List<String> facts = new ArrayList<>();
    private String testName = "";

    /** 60 dp of the sketch view, metres. */
    private double D;
    /** One dp of the sketch view, metres. */
    private double dpM;

    @Before
    public void startAtHome() {
        outDir = new File(InstrumentationRegistry.getInstrumentation().getTargetContext()
                .getExternalFilesDir(null), "evidence/cad-sketch-drafting");
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
            NativeViewport.sketchSetMultiSelect(false);
            NativeViewport.sketchSetDimensionVisibility(NativeViewport.DIM_VISIBILITY_SELECTED);
            NativeViewport.supportChooserCancel();
            NativeViewport.sketchCancel();
            NativeViewport.enterConstructionMode();
            return null;
        });
        resetToBaselineConstruction(rule.getScenario());
    }

    // =======================================================================
    // DEV-DR-01 — Construction
    // =======================================================================

    @Test
    public void devDr01_construction_removes_and_restores_the_fill_and_a_construction_line_is_a_revolve_axis() {
        testName = "dr01";
        newCadSketch();
        final double[] rect = drawRectangle(-2.5 * D, -0.5 * D, -0.5 * D, 0.5 * D);
        final long square = (long) rect[0];
        assertEquals("a regular rectangle fills: one area", 1, readyAreas());
        capture("01_regular_fill");

        selectTool(rule.getScenario(), R.id.tool_rail_select);
        // The top edge's middle: nothing is dimensioned, so no label stands anywhere.
        tapEntity(square, rect[1], rect[2] + 0.5 * rect[4]);
        assertArrayEquals("the tap selected the rectangle", new long[]{square}, selection());
        openPalette();
        assertEquals("the palette names what the toggle will do", string(R.string.sketch_make_construction),
                chipText(R.id.sketch_action_construction));
        assertNoBottomToolbar();
        capture("02_palette_one_selected");
        pressAction(R.id.sketch_action_construction);
        assertEquals("the rectangle is Construction", 1.0, entity(square)[NativeViewport.SKETCH_ENTITY_ROLE], 0.0);
        assertEquals(string(R.string.status_sketch_construction_on), statusLine());
        assertEquals(1, (int) draft()[NativeViewport.SKETCH_DRAFT_CONSTRUCTION_COUNT]);
        capture("03_construction_dashed");

        // The fill is gone: Finish has no closed profile to offer and says so.
        press(R.id.finish_sketch);
        fact("finish_construction_only", NativeViewport.sketchLastStatus() + " '" + statusLine() + "'");
        assertEquals("Finish is refused with no regular profile", NativeViewport.SKETCH_EDITING, sketchState());
        assertNotEquals(NativeViewport.CAD_OK, NativeViewport.sketchLastStatus());

        openPalette();
        assertEquals(string(R.string.sketch_make_regular), chipText(R.id.sketch_action_construction));
        pressAction(R.id.sketch_action_construction);
        assertEquals("Make Regular restores the role", 0.0, entity(square)[NativeViewport.SKETCH_ENTITY_ROLE], 0.0);
        assertEquals("and the fill", 1, readyAreas());

        // A construction centre line is a Revolve axis.
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        final long axis = placeLine(-3.0 * D, -D, -3.0 * D, D);
        openPalette();
        pressAction(R.id.sketch_action_construction);
        assertEquals(1.0, entity(axis)[NativeViewport.SKETCH_ENTITY_ROLE], 0.0);
        press(R.id.finish_sketch);
        assertEquals(NativeViewport.SKETCH_READY, sketchState());
        assertEquals("the construction line makes no area", 1,
                (int) sketchStateValue(NativeViewport.SKETCH_PROFILE_COUNT));
        pressInSketchEditor(R.id.sketch_revolve_begin);
        assertEquals(1.0, revolveState()[NativeViewport.REVOLVE_ACTIVE], 0.0);
        tapRevolveAxis(axis, -3.0 * D, 0.0);
        final double[] chosen = revolveState();
        assertEquals("the construction line is the axis", axis,
                (long) chosen[NativeViewport.REVOLVE_AXIS_ENTITY]);
        assertEquals(NativeViewport.CAD_OK, (int) chosen[NativeViewport.REVOLVE_CANDIDATE_STATUS]);
        final double area = rect[3] * rect[4];
        final double pappus = 2.0 * Math.PI * (rect[1] - (-3.0 * D)) * area;
        final double[] candidate = candidateMeasure();
        fact("revolve_candidate", Arrays.toString(candidate) + " pappus=" + pappus);
        assertEquals(pappus, candidate[NativeViewport.CANDIDATE_MEASURE_VOLUME], pappus * PAPPUS_TOLERANCE);
        capture("04_construction_axis_revolve");
        press(R.id.revolve_sketch);
        assertTrue("the Revolve created the project: " + statusLine(), NativeViewport.projectOpen());
        final long body = NativeViewport.sceneActiveBodyId();
        assertEquals("the committed body is the previewed one", candidate[NativeViewport.CANDIDATE_MEASURE_VOLUME],
                measure(body)[NativeViewport.CAD_MEASURE_VOLUME], 1e-9);
    }

    // =======================================================================
    // DEV-DR-02 — dimensions
    // =======================================================================

    @Test
    public void devDr02_dimensions_are_added_on_the_canvas_and_driving_values_drive_exact_geometry() {
        testName = "dr02";
        newCadSketch();
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        // A column of four, each a label band apart: the line's Length stands
        // above it and its Angle (0 degrees, under 45) below its start, the
        // rectangle's Width below it and its Height to its right, the circle's
        // diameter down and right -- so every tap below aims at a stroke no
        // label stands on, which is the product's own guarantee for the stroke
        // a label measures and this scene's spacing for its neighbours.
        final long line = placeLine(-3.0 * D, 2.8 * D, -1.5 * D, 2.8 * D);
        final double[] rect = drawRectangle(-3.0 * D, 0.4 * D, -1.5 * D, 1.4 * D);
        final double[] circle = drawCircle(-2.25 * D, -1.5 * D, 0.45 * D);
        final long arc = drawArc(-3.0 * D, -3.6 * D, -1.5 * D, -3.6 * D, -2.25 * D, -3.1 * D);
        final long rectangle = (long) rect[0];
        final long disk = (long) circle[0];

        selectTool(rule.getScenario(), R.id.tool_rail_select);
        tapEntity(line, -2.25 * D, 2.8 * D);
        openPalette();
        pressAction(R.id.sketch_action_dimension);
        assertEquals(NativeViewport.MODIFY_DIMENSION, (int) draft()[NativeViewport.SKETCH_DRAFT_MODE]);
        assertTrue("Length is offered for a line", shown(R.id.sketch_dimension_kind_length));
        assertTrue("Angle is offered for a line", shown(R.id.sketch_dimension_kind_angle));
        assertFalse("Radius is not", shown(R.id.sketch_dimension_kind_radius));
        assertTrue("Driving is the default", chipText(R.id.sketch_dimension_driving)
                .equals(string(R.string.sketch_dimension_driving)));
        capture("05_dimension_mode_line");
        press(R.id.sketch_dimension_kind_length);
        press(R.id.sketch_dimension_kind_angle);
        // In Dimension mode a tap on the drawing chooses the next target.
        tapEntity(rectangle, rect[1], rect[2] + 0.5 * rect[4]);
        assertEquals(rectangle, (long) draft()[NativeViewport.SKETCH_DRAFT_DIM_TARGET_ENTITY]);
        press(R.id.sketch_dimension_kind_width);
        press(R.id.sketch_dimension_kind_height);
        tapEntity(disk, circle[1], circle[2] + circle[3]);
        assertTrue(shown(R.id.sketch_dimension_kind_diameter));
        press(R.id.sketch_dimension_kind_diameter);
        final double[] arcValues = entity(arc);
        tapEntity(arc, arcValues[7], arcValues[8]);
        assertFalse("an arc has nothing to drive", shown(R.id.sketch_dimension_driving));
        press(R.id.sketch_dimension_kind_arc_radius);
        press(R.id.sketch_dimension_kind_sweep);
        press(R.id.sketch_modify_done);
        assertEquals(NativeViewport.MODIFY_NONE, (int) draft()[NativeViewport.SKETCH_DRAFT_MODE]);

        final double[][] dims = dimensions();
        fact("dimensions", Arrays.deepToString(dims));
        assertEquals("seven dimensions", 7, dims.length);
        final long lengthId = dimensionId(dims, NativeViewport.DIM_LINE_LENGTH);
        final long angleId = dimensionId(dims, NativeViewport.DIM_LINE_ANGLE);
        final long widthId = dimensionId(dims, NativeViewport.DIM_RECTANGLE_WIDTH);
        final long heightId = dimensionId(dims, NativeViewport.DIM_RECTANGLE_HEIGHT);
        final long diameterId = dimensionId(dims, NativeViewport.DIM_CIRCLE_DIAMETER);
        final long arcRadiusId = dimensionId(dims, NativeViewport.DIM_ARC_RADIUS);
        final long sweepId = dimensionId(dims, NativeViewport.DIM_ARC_SWEEP);
        for (long id : new long[]{lengthId, angleId, widthId, heightId, diameterId}) {
            assertEquals("driving " + id, NativeViewport.DIM_MODE_DRIVING, (int) dimension(dims, id)[NativeViewport.SKETCH_DIM_MODE]);
        }
        for (long id : new long[]{arcRadiusId, sweepId}) {
            assertEquals("reference " + id, NativeViewport.DIM_MODE_REFERENCE, (int) dimension(dims, id)[NativeViewport.SKETCH_DIM_MODE]);
        }
        setVisibility(R.id.sketch_dimensions_all);
        capture("06_all_dimensions");
        setVisibility(R.id.sketch_dimensions_selected);

        // Labels: Ø for a diameter, R and parentheses for a reference radius, ° for a sweep.
        focusEntity(disk);
        assertTrue(labelText(diameterId).startsWith("Ø "));
        focusEntity(arc);
        assertTrue(labelText(arcRadiusId).startsWith("(R "));
        assertTrue(labelText(sweepId).endsWith("°)"));

        // Line Length: P0 fixed, direction kept.
        focusEntity(line);
        final double[] before = entity(line);
        final double newLength = round3(1.8 * D);
        typeOnLabel(lengthId, LengthUnit.present(java.math.BigDecimal.valueOf(newLength)));
        final double[] afterLength = entity(line);
        assertEquals(before[2], afterLength[2], 0.0);
        assertEquals(before[3], afterLength[3], 0.0);
        assertEquals(newLength, Math.hypot(afterLength[4] - afterLength[2], afterLength[5] - afterLength[3]), EXACT);
        assertEquals(string(R.string.status_sketch_dimension_applied), statusLine());

        // Line Angle: 30 degrees, length kept.
        typeOnLabel(angleId, "30");
        final double[] afterAngle = entity(line);
        assertEquals(30.0, Math.toDegrees(Math.atan2(afterAngle[5] - afterAngle[3], afterAngle[4] - afterAngle[2])), 1e-9);
        assertEquals(newLength, Math.hypot(afterAngle[4] - afterAngle[2], afterAngle[5] - afterAngle[3]), EXACT);
        assertEquals("the label reads the typed angle", "30°", labelText(angleId));

        // Rectangle Width / Height, centre kept.
        focusEntity(rectangle);
        final double width = round3(1.7 * D);
        final double height = round3(0.8 * D);
        typeOnLabel(widthId, LengthUnit.present(java.math.BigDecimal.valueOf(width)));
        typeOnLabel(heightId, LengthUnit.present(java.math.BigDecimal.valueOf(height)));
        final double[] r2 = entity(rectangle);
        assertEquals(rect[1], r2[2], EXACT);
        assertEquals(rect[2], r2[3], EXACT);
        assertEquals(width, r2[4], EXACT);
        assertEquals(height, r2[5], EXACT);

        // Circle Diameter.
        focusEntity(disk);
        final double diameter = round3(1.1 * D);
        typeOnLabel(diameterId, LengthUnit.present(java.math.BigDecimal.valueOf(diameter)));
        assertEquals(diameter / 2.0, entity(disk)[4], EXACT);
        assertEquals("Ø " + LengthUnit.METERS.formatWithUnit(diameter), labelText(diameterId));

        // The arc's radius and sweep are Reference: measured, never typed.
        focusEntity(arc);
        final View field = openLabelEditor(arcRadiusId);
        assertFalse("a reference label opens no value field", field.isShown());
        assertTrue("only Delete", shown(R.id.sketch_dimension_delete));
        closeLabelEditor();
        assertEquals(NativeViewport.CAD_SKETCH_DIMENSION_READ_ONLY,
                applyOnUi(() -> NativeViewport.sketchApplyDimensionValue(sweepId, 90.0)));
        capture("07_driven_geometry");
    }

    // =======================================================================
    // DEV-DR-03 — technical drawing visibility
    // =======================================================================

    @Test
    public void devDr03_selected_all_off_and_labels_stay_attached_through_zoom_and_pan() {
        testName = "dr03";
        newCadSketch();
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        final long line = placeLine(-3.0 * D, 2.4 * D, -1.6 * D, 3.0 * D);
        final double[] rect = drawRectangle(-3.0 * D, 0.2 * D, -1.4 * D, 1.2 * D);
        final double[] circle = drawCircle(-2.2 * D, -1.4 * D, 0.45 * D);
        final long arc = drawArc(-3.0 * D, -3.4 * D, -1.5 * D, -3.4 * D, -2.25 * D, -2.9 * D);
        final long rectangle = (long) rect[0];
        final long disk = (long) circle[0];
        assertEquals(NativeViewport.CAD_OK, applyOnUi(() -> {
            int s = NativeViewport.sketchSetModifyMode(NativeViewport.MODIFY_DIMENSION);
            s |= NativeViewport.sketchSetDimensionTarget(line, 0);
            s |= NativeViewport.sketchAddDimension(NativeViewport.DIM_LINE_LENGTH, NativeViewport.DIM_MODE_DRIVING);
            s |= NativeViewport.sketchAddDimension(NativeViewport.DIM_LINE_ANGLE, NativeViewport.DIM_MODE_DRIVING);
            s |= NativeViewport.sketchAddDimension(NativeViewport.DIM_LINE_HORIZONTAL, NativeViewport.DIM_MODE_REFERENCE);
            s |= NativeViewport.sketchAddDimension(NativeViewport.DIM_LINE_VERTICAL, NativeViewport.DIM_MODE_REFERENCE);
            s |= NativeViewport.sketchSetDimensionTarget(rectangle, 0);
            s |= NativeViewport.sketchAddDimension(NativeViewport.DIM_RECTANGLE_WIDTH, NativeViewport.DIM_MODE_DRIVING);
            s |= NativeViewport.sketchAddDimension(NativeViewport.DIM_RECTANGLE_HEIGHT, NativeViewport.DIM_MODE_DRIVING);
            s |= NativeViewport.sketchSetDimensionTarget(disk, 0);
            s |= NativeViewport.sketchAddDimension(NativeViewport.DIM_CIRCLE_DIAMETER, NativeViewport.DIM_MODE_DRIVING);
            s |= NativeViewport.sketchSetDimensionTarget(arc, 0);
            s |= NativeViewport.sketchAddDimension(NativeViewport.DIM_ARC_RADIUS, NativeViewport.DIM_MODE_REFERENCE);
            s |= NativeViewport.sketchAddDimension(NativeViewport.DIM_ARC_SWEEP, NativeViewport.DIM_MODE_REFERENCE);
            s |= NativeViewport.sketchSetModifyMode(NativeViewport.MODIFY_NONE);
            return s;
        }));
        assertEquals("nine dimensions", 9, dimensions().length);
        // Empty drawing, a label band clear of the rectangle and the circle.
        clearSelectionByTap(-2.9 * D, -0.6 * D);

        // Selected (the default) with nothing selected: no labels at all.
        assertEquals(NativeViewport.DIM_VISIBILITY_SELECTED, (int) draft()[NativeViewport.SKETCH_DRAFT_DIM_VISIBILITY]);
        assertEquals(0, nativeLabelCount());
        tapEntity(line, -2.3 * D, 2.7 * D);
        assertEquals("Selected shows the line's four", 4, nativeLabelCount());
        assertCollisionPolicy("selected", 4);
        capture("08_visibility_selected");

        setVisibility(R.id.sketch_dimensions_all);
        assertEquals("All shows nine", 9, nativeLabelCount());
        fact("all.visible_chips", visibleLabelCount());
        assertCollisionPolicy("all", 9);
        assertNoChipOverlap();
        assertLabelsAttached("all");
        capture("09_visibility_all");

        setVisibility(R.id.sketch_dimensions_off);
        assertEquals(0, nativeLabelCount());
        assertEquals(0, visibleLabelCount());
        capture("10_visibility_off");
        setVisibility(R.id.sketch_dimensions_all);

        // A real pinch and a real two-finger pan: every label follows its
        // anchor and the collision policy holds at every view.
        pinch(1.35f);
        assertLabelsAttached("pinch_out");
        assertCollisionPolicy("pinch_out", 9);
        assertNoChipOverlap();
        pinch(0.8f);
        assertLabelsAttached("pinch_in");
        assertCollisionPolicy("pinch_in", 9);
        twoFingerPan(-30f, 40f);
        assertLabelsAttached("pan");
        assertCollisionPolicy("pan", 9);
        assertNoChipOverlap();
        capture("11_after_zoom_pan");
    }

    // =======================================================================
    // DEV-DR-04 — snaps
    // =======================================================================

    @Test
    public void devDr04_real_drawing_touches_land_on_every_snap_kind() {
        testName = "dr04";
        newCadSketch();
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        final double ref = 1.5 * D;

        // Endpoint and Midpoint of one reference line.
        final long r = placeLine(-3.0 * D, ref, -2.0 * D, ref);
        snapCase("endpoint", -1.0 * D, -1.5 * D, -2.0 * D, ref, 0f, 0f, NativeViewport.SNAP_ENDPOINT,
                -2.0 * D, ref);
        snapCase("midpoint", -1.0 * D, -1.5 * D, -2.5 * D, ref, 0f, 0f, NativeViewport.SNAP_MIDPOINT,
                -2.5 * D, ref);
        clearSketch();

        // Center of a circle.
        final double[] circle = drawCircle(-2.0 * D, 0.0, 0.5 * D);
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        snapCase("center", -0.5 * D, -1.5 * D, circle[1], circle[2], 0f, 0f, NativeViewport.SNAP_CENTER,
                circle[1], circle[2]);
        clearSketch();

        // Intersection of two crossing lines (also both midpoints: priority).
        placeLine(-3.0 * D, -D, -D, D);
        placeLine(-3.0 * D, D, -D, -D);
        snapCase("intersection", -0.5 * D, -2.0 * D, -2.0 * D, 0.0, 0f, 0f,
                NativeViewport.SNAP_INTERSECTION, -2.0 * D, 0.0);
        clearSketch();

        // The sketch origin.
        snapCase("origin", -1.5 * D, -1.5 * D, 0.0, 0.0, 0f, 0f, NativeViewport.SNAP_ORIGIN, 0.0, 0.0);
        clearSketch();

        // A horizontal guide from a far endpoint (4 dp off the exact v).
        placeLine(-3.0 * D, D, -2.0 * D, D);
        snapCase("h_guide", -D, -1.5 * D, -0.5 * D, D, 0f, -4f, NativeViewport.SNAP_HORIZONTAL_GUIDE,
                Double.NaN, D);
        clearSketch();

        // A vertical guide from a far endpoint (4 dp off the exact u).
        placeLine(-2.0 * D, 1.5 * D, -2.0 * D, 2.5 * D);
        snapCase("v_guide", -0.5 * D, -2.0 * D, -2.0 * D, -0.5 * D, 4f, 0f,
                NativeViewport.SNAP_VERTICAL_GUIDE, -2.0 * D, Double.NaN);
        fact("reference_line", r);
    }

    // =======================================================================
    // DEV-DR-05 — Trim
    // =======================================================================

    @Test
    public void devDr05_real_taps_trim_a_line_a_circle_and_a_rectangle_edge_and_the_fill_follows() {
        testName = "dr05";
        newCadSketch();
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        final long h = placeLine(-2.5 * D, 0.0, D, 0.0);
        final double[] rect = drawRectangle(-2.25 * D, -0.5 * D, -1.25 * D, 0.5 * D);
        final double[] circle = drawCircle(0.0, 0.0, 0.5 * D);
        // The crossing points must be exact on v = 0: the line is re-typed
        // through the rectangle's and circle's own centres' v.
        assertEquals("the rectangle stands on the line", 0.0, rect[2], EXACT);
        assertEquals("the circle stands on the line", 0.0, circle[2], EXACT);
        final double rectRight = rect[1] + 0.5 * rect[3];
        final int areas0 = readyAreas();
        fact("areas.before", areas0);

        openPalette();
        pressAction(R.id.sketch_action_trim);
        assertEquals(NativeViewport.MODIFY_TRIM, (int) draft()[NativeViewport.SKETCH_DRAFT_MODE]);
        capture("12_trim_mode");
        // 1. The line's segment between the rectangle and the circle.
        final double gapU = 0.5 * (rectRight + (circle[1] - circle[3]));
        tapAt(gapU, 0.0);
        assertEquals(string(R.string.status_sketch_trimmed), statusLine());
        final double[] hAfter = entity(h);
        fact("trim.line", Arrays.toString(hAfter));
        assertEquals("the line keeps its start", -2.5 * D, hAfter[2], EXACT);
        assertEquals("and now ends on the rectangle", rectRight, hAfter[4], EXACT);
        final long piece = newestEntity(NativeViewport.SKETCH_ENTITY_KIND_LINE, h);
        final double[] p = entity(piece);
        assertEquals("the other piece starts on the circle", circle[1] - circle[3], p[2], EXACT);
        assertEquals(D, p[4], EXACT);
        // 2. The circle's upper arc.
        tapAt(circle[1], circle[3]);
        assertNull("the circle is gone", entityOrNull((long) circle[0]));
        final long arc = newestEntity(NativeViewport.SKETCH_ENTITY_KIND_ARC, 0);
        final double[] a = entity(arc);
        fact("trim.arc", Arrays.toString(a));
        assertEquals("the arc ends on the line", 0.0, a[3], EXACT);
        assertEquals(0.0, a[5], EXACT);
        assertEquals(circle[1] - circle[3], Math.min(a[2], a[4]), EXACT);
        assertEquals(circle[1] + circle[3], Math.max(a[2], a[4]), EXACT);
        assertTrue("the kept arc is the lower one", a[8] < 0.0);
        // 3. The rectangle's top edge.
        tapAt(rect[1] + 0.2 * rect[3], rect[2] + 0.5 * rect[4]);
        assertEquals(string(R.string.status_sketch_trim_rectangle), statusLine());
        assertNull("the rectangle became lines", entityOrNull((long) rect[0]));
        capture("13_trimmed");
        press(R.id.sketch_modify_done);
        final int areas = readyAreas();
        fact("areas.after", areas);
        assertEquals("before: the line splits the rectangle and the circle", 4, areas0);
        assertEquals("after: the closed lower halves only", 2, areas);
    }

    // =======================================================================
    // DEV-DR-06 — dimension dependency
    // =======================================================================

    @Test
    public void devDr06_a_dimension_refuses_trim_and_a_driving_length_refuses_extend_until_it_is_removed() {
        testName = "dr06";
        newCadSketch();
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        final long cross = placeLine(-1.6 * D, -D, -1.6 * D, D);
        final double[] circle = drawCircle(0.0, 0.0, 0.5 * D);
        assertEquals(0.0, circle[2], EXACT);
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        final long line = placeLine(-2.5 * D, 0.0, -D, 0.0);
        // Dimension the line, Driving, from the palette.
        openPalette();
        pressAction(R.id.sketch_action_dimension);
        press(R.id.sketch_dimension_kind_length);
        press(R.id.sketch_modify_done);
        final long lengthId = dimensionId(dimensions(), NativeViewport.DIM_LINE_LENGTH);

        openPalette();
        pressAction(R.id.sketch_action_trim);
        final double[] before = entity(line);
        tapAt(-2.2 * D, 0.0);
        assertEquals("Trim is refused by name", string(R.string.status_sketch_dimension_dependency), statusLine());
        assertArrayEquals("the line is untouched", before, entity(line), 0.0);
        press(R.id.sketch_modify_done);

        openPalette();
        pressAction(R.id.sketch_action_extend);
        tapAt(-1.15 * D, 0.0);
        assertEquals("Extend is refused by name", string(R.string.status_sketch_dimension_locked), statusLine());
        assertArrayEquals(before, entity(line), 0.0);
        press(R.id.sketch_modify_done);
        capture("14_dependency_refusals");

        // Delete the dimension from its own label.
        selectTool(rule.getScenario(), R.id.tool_rail_select);
        tapEntity(line, -1.8 * D, 0.0);
        openLabelEditor(lengthId);
        press(R.id.sketch_dimension_delete);
        assertEquals(0, dimensions().length);
        assertEquals(string(R.string.status_sketch_dimension_removed), statusLine());

        openPalette();
        pressAction(R.id.sketch_action_extend);
        tapAt(-1.15 * D, 0.0);
        assertEquals(string(R.string.status_sketch_extended), statusLine());
        assertEquals("extended to the circle", circle[1] - circle[3], entity(line)[4], EXACT);
        press(R.id.sketch_modify_done);
        openPalette();
        pressAction(R.id.sketch_action_trim);
        tapAt(-2.2 * D, 0.0);
        assertEquals(string(R.string.status_sketch_trimmed), statusLine());
        assertEquals("the stub left of the crossing line went", -1.6 * D, entity(line)[2], EXACT);
        fact("cross", cross);
    }

    // =======================================================================
    // DEV-DR-07 — Extend
    // =======================================================================

    @Test
    public void devDr07_a_real_tap_extends_a_line_exactly_to_a_line_and_to_a_circle() {
        testName = "dr07";
        newCadSketch();
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        placeLine(0.0, 0.4 * D, 0.0, 2.0 * D);
        final long toLine = placeLine(-2.5 * D, 1.3 * D, -1.5 * D, 1.3 * D);
        final double[] circle = drawCircle(-0.2 * D, -1.3 * D, 0.5 * D);
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        // The line just drawn stays selected, as it does for a user: Extend is
        // offered with it, and its length label stands above it, clear of the
        // stroke the second tap lands on.
        final long toCircle = placeLine(-2.5 * D, circle[2], -1.5 * D, circle[2]);
        assertArrayEquals(new long[]{toCircle}, selection());
        openPalette();
        pressAction(R.id.sketch_action_extend);
        capture("15_extend_mode");
        tapAt(-1.6 * D, 1.3 * D);
        assertEquals(string(R.string.status_sketch_extended), statusLine());
        final double[] a = entity(toLine);
        assertEquals("to the vertical line, exactly", 0.0, a[4], EXACT);
        assertEquals(1.3 * D, a[5], EXACT);
        tapAt(-1.6 * D, circle[2]);
        final double[] b = entity(toCircle);
        assertEquals("to the circle, exactly", circle[1] - circle[3], b[4], EXACT);
        assertEquals(circle[2], b[5], EXACT);
        press(R.id.sketch_modify_done);
        capture("16_extended");
    }

    // =======================================================================
    // DEV-DR-08 — Offset
    // =======================================================================

    @Test
    public void devDr08_offset_previews_follows_a_drag_takes_an_exact_value_confirms_and_cancels() {
        testName = "dr08";
        newCadSketch();
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        final long line = placeLine(-3.0 * D, 2.2 * D, -1.5 * D, 2.2 * D);
        final double[] rect = drawRectangle(-3.0 * D, 0.5 * D, -1.5 * D, 1.2 * D);
        final double[] circle = drawCircle(-2.25 * D, -0.5 * D, 0.4 * D);
        selectTool(rule.getScenario(), R.id.tool_rail_polyline);
        tapSketchReal(-3.0 * D, -2.4 * D);
        tapSketchReal(-2.0 * D, -2.4 * D);
        tapSketchReal(-2.0 * D, -1.5 * D);
        tapSketchReal(-2.0 * D, -1.5 * D);
        final long polyline = newestEntity(NativeViewport.SKETCH_ENTITY_KIND_POLYLINE, 0);
        final double[] poly = entity(polyline);
        assertEquals("an open three-vertex polyline", 3.0, poly[2], 0.0);
        assertEquals(0.0, poly[3], 0.0);

        // Line: a drag, then an exact value.
        selectTool(rule.getScenario(), R.id.tool_rail_select);
        tapEntity(line, -2.6 * D, 2.2 * D);
        openPalette();
        pressAction(R.id.sketch_action_offset);
        assertEquals(line, (long) draft()[NativeViewport.SKETCH_DRAFT_OFFSET_SOURCE]);
        assertEquals("the default preview is valid", NativeViewport.CAD_OK, (int) draft()[NativeViewport.SKETCH_DRAFT_OFFSET_STATUS]);
        assertTrue("Confirm stands over a valid preview", shown(R.id.sketch_modify_confirm));
        // Up, toward the line's left (+v) side, ending wherever the finger
        // ends -- over the line's own length label too: the drag belongs to
        // the viewport that took its Down.
        realDrag(-2.25 * D, 2.2 * D, -2.25 * D, 2.7 * D);
        final double dragged = draft()[NativeViewport.SKETCH_DRAFT_OFFSET_DISTANCE];
        fact("offset.dragged", dragged);
        assertTrue("the drag set a positive (left) distance: " + dragged,
                dragged > 0.2 * D && dragged < 0.8 * D);
        assertEquals("a drag creates nothing", 4, sketchEntityCount());
        capture("17_offset_preview_drag");
        typeOffset("0.123");
        assertEquals(0.123, draft()[NativeViewport.SKETCH_DRAFT_OFFSET_DISTANCE], 0.0);
        press(R.id.sketch_modify_confirm);
        assertEquals(string(R.string.status_sketch_offset, LengthUnit.METERS.formatWithUnit(0.123)), statusLine());
        final long offsetLine = newestEntity(NativeViewport.SKETCH_ENTITY_KIND_LINE, line);
        final double[] ol = entity(offsetLine);
        assertEquals(-3.0 * D, ol[2], EXACT);
        assertEquals(2.2 * D + 0.123, ol[3], EXACT);
        assertEquals(-1.5 * D, ol[4], EXACT);
        assertEquals(2.2 * D + 0.123, ol[5], EXACT);

        // Cancel creates nothing.
        final int count = sketchEntityCount();
        selectByTap((long) circle[0], circle[1] - circle[3], circle[2]);
        openPalette();
        pressAction(R.id.sketch_action_offset);
        typeOffset("0.05");
        press(R.id.sketch_modify_done);
        assertEquals("Cancel created nothing", count, sketchEntityCount());
        assertEquals(NativeViewport.MODIFY_NONE, (int) draft()[NativeViewport.SKETCH_DRAFT_MODE]);

        // Circle, outward.
        selectByTap((long) circle[0], circle[1] - circle[3], circle[2]);
        openPalette();
        pressAction(R.id.sketch_action_offset);
        typeOffset("0.05");
        press(R.id.sketch_modify_confirm);
        final double[] oc = entity(newestEntity(NativeViewport.SKETCH_ENTITY_KIND_CIRCLE, (long) circle[0]));
        assertEquals(circle[1], oc[2], EXACT);
        assertEquals(circle[2], oc[3], EXACT);
        assertEquals(circle[3] + 0.05, oc[4], EXACT);

        // Rectangle, outward: its left edge's middle.
        selectByTap((long) rect[0], rect[1] - 0.5 * rect[3], rect[2]);
        openPalette();
        pressAction(R.id.sketch_action_offset);
        typeOffset("0.04");
        press(R.id.sketch_modify_confirm);
        final double[] orr = entity(newestEntity(NativeViewport.SKETCH_ENTITY_KIND_RECTANGLE, (long) rect[0]));
        assertEquals(rect[1], orr[2], EXACT);
        assertEquals(rect[2], orr[3], EXACT);
        assertEquals(rect[3] + 0.08, orr[4], EXACT);
        assertEquals(rect[4] + 0.08, orr[5], EXACT);

        // Polyline: the left side of +u then +v is +v then -u. Its first
        // segment's middle.
        selectByTap(polyline, -2.5 * D, -2.4 * D);
        openPalette();
        pressAction(R.id.sketch_action_offset);
        typeOffset("0.05");
        press(R.id.sketch_modify_confirm);
        final double[] op = entity(newestEntity(NativeViewport.SKETCH_ENTITY_KIND_POLYLINE, polyline));
        fact("offset.polyline", Arrays.toString(op) + " source " + Arrays.toString(poly));
        assertEquals(3.0, op[2], 0.0);
        assertEquals("first vertex: up by d", poly[7], op[7], EXACT);
        assertEquals(poly[8] + 0.05, op[8], EXACT);
        assertEquals("last vertex: left by d", poly[4] - 0.05, op[4], EXACT);
        assertEquals(poly[5], op[5], EXACT);
        assertEquals("no offset created a dimension", 0, dimensions().length);
        capture("18_offsets");
    }

    // =======================================================================
    // DEV-DR-09 — Mirror
    // =======================================================================

    @Test
    public void devDr09_three_selected_items_mirror_across_a_construction_line_without_their_dimensions() {
        testName = "dr09";
        newCadSketch();
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        final long axis = placeLine(-1.5 * D, -2.0 * D, -1.5 * D, 2.0 * D);
        openPalette();
        pressAction(R.id.sketch_action_construction);
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        final long line = placeLine(-3.0 * D, 1.6 * D, -2.1 * D, 1.0 * D);
        final double[] circle = drawCircle(-2.5 * D, 0.0, 0.3 * D);
        final double[] rect = drawRectangle(-2.9 * D, -1.5 * D, -2.1 * D, -0.9 * D);
        // A dimension on the line, which a mirror must not copy.
        assertEquals(NativeViewport.CAD_OK, applyOnUi(() -> {
            int s = NativeViewport.sketchSetModifyMode(NativeViewport.MODIFY_DIMENSION);
            s |= NativeViewport.sketchSetDimensionTarget(line, 0);
            s |= NativeViewport.sketchAddDimension(NativeViewport.DIM_LINE_LENGTH, NativeViewport.DIM_MODE_DRIVING);
            s |= NativeViewport.sketchSetModifyMode(NativeViewport.MODIFY_NONE);
            return s;
        }));
        // Empty drawing above the line's upper end, well clear of the axis.
        clearSelectionByTap(-2.9 * D, 2.6 * D);

        openPalette();
        press(R.id.sketch_action_select_multiple);
        assertEquals(1.0, draft()[NativeViewport.SKETCH_DRAFT_MULTI_SELECT], 0.0);
        closePalette();
        tapEntity(line, -2.55 * D, 1.3 * D);
        tapEntity((long) circle[0], circle[1], circle[2] + circle[3]);
        tapEntity((long) rect[0], rect[1], rect[2] - 0.5 * rect[4]);
        assertEquals("three selected", 3, selection().length);
        assertEquals(string(R.string.status_sketch_selection_count, 3), statusLine());
        capture("19_multi_select");

        openPalette();
        pressAction(R.id.sketch_action_mirror);
        assertEquals(NativeViewport.MODIFY_MIRROR, (int) draft()[NativeViewport.SKETCH_DRAFT_MODE]);
        assertFalse("no Confirm before an axis", shown(R.id.sketch_modify_confirm));
        tapEntityAt(axis, -1.5 * D, 0.6 * D);
        assertEquals(1.0, draft()[NativeViewport.SKETCH_DRAFT_MIRROR_AXIS_SET], 0.0);
        assertEquals(axis, (long) draft()[NativeViewport.SKETCH_DRAFT_MIRROR_AXIS_ENTITY]);
        assertTrue("Confirm stands over the preview", shown(R.id.sketch_modify_confirm));
        capture("20_mirror_preview");
        final int before = sketchEntityCount();
        press(R.id.sketch_modify_confirm);
        assertEquals(string(R.string.status_sketch_mirrored), statusLine());
        assertEquals("three new items", before + 3, sketchEntityCount());
        final double m = -3.0 * D;  // u' = 2 * axisU - u
        final double[] ml = entity(newestEntity(NativeViewport.SKETCH_ENTITY_KIND_LINE, line));
        final double[] src = entity(line);
        assertEquals(m - src[2], ml[2], EXACT);
        assertEquals(src[3], ml[3], EXACT);
        assertEquals(m - src[4], ml[4], EXACT);
        assertEquals(src[5], ml[5], EXACT);
        final double[] mc = entity(newestEntity(NativeViewport.SKETCH_ENTITY_KIND_CIRCLE, (long) circle[0]));
        assertEquals(m - circle[1], mc[2], EXACT);
        assertEquals(circle[3], mc[4], EXACT);
        final double[] mr = entity(newestEntity(NativeViewport.SKETCH_ENTITY_KIND_RECTANGLE, (long) rect[0]));
        assertEquals("a rectangle across a vertical axis stays a rectangle", m - rect[1], mr[2], EXACT);
        assertEquals(rect[3], mr[4], EXACT);
        assertEquals(rect[4], mr[5], EXACT);
        assertEquals("the dimension was not copied", 1, dimensions().length);
        openPalette();
        press(R.id.sketch_action_select_multiple);
        closePalette();
        capture("21_mirrored");
    }

    // =======================================================================
    // DEV-DR-10 — save / reopen
    // =======================================================================

    @Test
    public void devDr10_a_saved_v8_project_reopens_with_exact_roles_and_dimensions_and_no_selection() {
        testName = "dr10";
        newCadSketch();
        final double[] rect = drawRectangle(-2.75 * D, -0.5 * D, -1.25 * D, 0.5 * D);
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        final long diagonal = placeLine(-3.0 * D, -1.0 * D, -0.5 * D, 1.5 * D);
        openPalette();
        pressAction(R.id.sketch_action_construction);
        // Width and height Driving; the construction line's length Reference,
        // chosen through the Driving / Reference chip. Each entity is chosen
        // by a real tap: the rectangle on its bottom edge, right of where the
        // diagonal crosses it, the diagonal on its upper run, clear of the
        // rectangle's own labels.
        selectByTap((long) rect[0], rect[1] + 0.3 * rect[3], rect[2] - 0.5 * rect[4]);
        openPalette();
        pressAction(R.id.sketch_action_dimension);
        press(R.id.sketch_dimension_kind_width);
        press(R.id.sketch_dimension_kind_height);
        selectByTap(diagonal, -1.125 * D, 0.875 * D);
        openPalette();
        pressAction(R.id.sketch_action_dimension);
        press(R.id.sketch_dimension_driving);
        assertEquals(string(R.string.sketch_dimension_reference), chipText(R.id.sketch_dimension_driving));
        press(R.id.sketch_dimension_kind_length);
        press(R.id.sketch_modify_done);
        final double[][] dims = dimensions();
        fact("dims.before", Arrays.deepToString(dims));
        assertEquals(3, dims.length);
        final long[] ids = entityIds();
        final double[][] entities = new double[ids.length][];
        for (int i = 0; i < ids.length; i++) entities[i] = entity(ids[i]);

        press(R.id.finish_sketch);
        assertEquals(NativeViewport.SKETCH_READY, sketchState());
        press(R.id.extrude_sketch);
        assertTrue("the extrusion created the project: " + statusLine(), NativeViewport.projectOpen());
        final long body = NativeViewport.sceneActiveBodyId();

        // Save through the Project surface; the slot carries CADB v8.
        final Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        context.deleteFile(ProjectSlot.SLOT_FILE_NAME);
        press(R.id.project_actions_button);
        press(R.id.project_save);
        assertTrue("Save wrote the slot", ProjectSlot.exists(context));
        final byte[] saved = readSlot(context);
        assertEquals("a drafting project is CADB v8", 8, cadbVersion(saved));

        // Reopen.
        assertEquals(NativeViewport.PROJECT_OK, (int) onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int status = NativeViewport.loadProject(saved);
            workspace.onNativeStateChanged();
            return status;
        }));
        settleLayout();
        final long reopened = NativeViewport.sceneActiveBodyId();
        assertEquals("the same identity", body, reopened);
        editSketchFromCanvas(reopened);
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
        assertArrayEquals("the same entities", ids, entityIds());
        for (int i = 0; i < ids.length; i++) {
            assertArrayEquals("entity " + ids[i] + " exact, role included", entities[i], entity(ids[i]), 0.0);
        }
        final double[][] reopenedDims = dimensions();
        assertEquals(dims.length, reopenedDims.length);
        for (int i = 0; i < dims.length; i++) {
            assertArrayEquals("dimension record " + i + " exact", dims[i], reopenedDims[i], 0.0);
        }
        final double[] state = draft();
        assertEquals("no selection persisted", 0, (int) state[NativeViewport.SKETCH_DRAFT_SELECTION_COUNT]);
        assertEquals(NativeViewport.MODIFY_NONE, (int) state[NativeViewport.SKETCH_DRAFT_MODE]);
        assertEquals(0.0, state[NativeViewport.SKETCH_DRAFT_MULTI_SELECT], 0.0);

        final long widthId = dimensionId(reopenedDims, NativeViewport.DIM_RECTANGLE_WIDTH);
        final long refId = dimensionId(reopenedDims, NativeViewport.DIM_LINE_LENGTH);
        focusEntity(diagonal);
        assertTrue("the reference label reads in parentheses", labelText(refId).startsWith("("));
        assertFalse("a reference label opens no value", openLabelEditor(refId).isShown());
        closeLabelEditor();
        focusEntity((long) rect[0]);
        assertTrue("a driving label opens its value", openLabelEditor(widthId).isShown());
        closeLabelEditor();
        capture("22_reopened_labels");
    }

    // =======================================================================
    // DEV-DR-11 — dependent CAD
    // =======================================================================

    @Test
    public void devDr11a_an_extruded_body_regenerates_from_a_drafting_edit_and_refuses_a_lost_profile_by_name() {
        testName = "dr11a";
        newCadSketch();
        final double[] rect = drawRectangle(-2.75 * D, -0.5 * D, -1.25 * D, 0.5 * D);
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        final long diagonal = placeLine(-3.0 * D, -1.0 * D, -0.5 * D, 1.5 * D);
        openPalette();
        pressAction(R.id.sketch_action_construction);
        selectByTap((long) rect[0], rect[1] + 0.3 * rect[3], rect[2] - 0.5 * rect[4]);
        openPalette();
        pressAction(R.id.sketch_action_dimension);
        press(R.id.sketch_dimension_kind_width);
        press(R.id.sketch_modify_done);
        press(R.id.finish_sketch);
        assertEquals(NativeViewport.SKETCH_READY, sketchState());
        final double depth = sketchStateValue(NativeViewport.SKETCH_EXTRUDE_DEPTH);
        press(R.id.extrude_sketch);
        final long body = NativeViewport.sceneActiveBodyId();
        final double v0 = rect[3] * rect[4] * depth;
        assertEquals(v0, measure(body)[NativeViewport.CAD_MEASURE_VOLUME], 1e-6 * v0);

        // A drafting edit: a new driving width, then a trim of the construction
        // diagonal's stub below the rectangle. One Finish, one Extrude.
        editSketchFromCanvas(body);
        focusEntity((long) rect[0]);
        final double width = round3(1.9 * D);
        typeOnLabel(dimensionId(dimensions(), NativeViewport.DIM_RECTANGLE_WIDTH),
                LengthUnit.present(java.math.BigDecimal.valueOf(width)));
        // Trim is offered with the rectangle still selected. The stub runs
        // from the diagonal's start to where it crosses the widened
        // rectangle's bottom edge; the tap is on it, nearer the diagonal than
        // the edge, above the Width label standing below that edge.
        openPalette();
        pressAction(R.id.sketch_action_trim);
        tapAt(-2.6 * D, -0.6 * D);
        assertEquals(string(R.string.status_sketch_trimmed), statusLine());
        press(R.id.sketch_modify_done);
        press(R.id.finish_sketch);
        assertEquals(NativeViewport.SKETCH_READY, sketchState());
        press(R.id.extrude_sketch);
        assertEquals(NativeViewport.SKETCH_INACTIVE, sketchState());
        assertEquals("one body still", 1, NativeViewport.sceneBodyCount());
        assertEquals("the same body regenerated", body, NativeViewport.sceneActiveBodyId());
        final double regenerated = measure(body)[NativeViewport.CAD_MEASURE_VOLUME];
        fact("regenerated", regenerated);
        assertEquals(width * rect[4] * depth, regenerated, 1e-6 * regenerated);
        capture("23_regenerated");

        // Making the only profile Construction is refused by name at Finish.
        editSketchFromCanvas(body);
        final double[] wide = entity((long) rect[0]);
        selectByTap((long) rect[0], wide[2], wide[3] + 0.5 * wide[5]);
        openPalette();
        pressAction(R.id.sketch_action_construction);
        press(R.id.finish_sketch);
        assertEquals("Finish refused: no regular profile", NativeViewport.SKETCH_EDITING, sketchState());
        assertNotEquals(NativeViewport.CAD_OK, NativeViewport.sketchLastStatus());
        fact("lost_profile", NativeViewport.sketchLastStatus() + " '" + statusLine() + "'");
        press(R.id.cancel_sketch);
        assertEquals("Cancel ends the edit", NativeViewport.SKETCH_INACTIVE, sketchState());
        assertEquals("Cancel cost the body nothing", regenerated,
                measure(body)[NativeViewport.CAD_MEASURE_VOLUME], 0.0);
        fact("diagonal", diagonal);
    }

    @Test
    public void devDr11b_a_revolved_body_regenerates_from_a_driving_dimension_edit() {
        testName = "dr11b";
        newCadSketch();
        final double[] square = drawRectangle(-2.0 * D, -0.5 * D, -D, 0.5 * D);
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        final long axis = placeLine(-3.0 * D, -D, -3.0 * D, D);
        openPalette();
        pressAction(R.id.sketch_action_construction);
        // The square's right edge's middle: the selected axis's own length
        // label stands at the left edge of the view, beside the axis.
        selectByTap((long) square[0], square[1] + 0.5 * square[3], square[2]);
        openPalette();
        pressAction(R.id.sketch_action_dimension);
        press(R.id.sketch_dimension_kind_width);
        press(R.id.sketch_modify_done);
        press(R.id.finish_sketch);
        pressInSketchEditor(R.id.sketch_revolve_begin);
        tapRevolveAxis(axis, -3.0 * D, 0.0);
        press(R.id.revolve_sketch);
        assertTrue(NativeViewport.projectOpen());
        final long body = NativeViewport.sceneActiveBodyId();
        final double r = square[1] - (-3.0 * D);
        final double v0 = measure(body)[NativeViewport.CAD_MEASURE_VOLUME];
        assertEquals(2.0 * Math.PI * r * square[3] * square[4], v0, v0 * PAPPUS_TOLERANCE);

        // A revolved body carries no extrude arrow, so no canvas Edit Sketch
        // control stands over it (`CAD-V6-REVOLVE-NEWBODY-E2E-R1`); its retained
        // sketch is reached from the precision surface's Edit Sketch, the path
        // a user takes.
        editSketchFromInspector();
        focusEntity((long) square[0]);
        final double width = round3(1.4 * D);
        typeOnLabel(dimensionId(dimensions(), NativeViewport.DIM_RECTANGLE_WIDTH),
                LengthUnit.present(java.math.BigDecimal.valueOf(width)));
        press(R.id.finish_sketch);
        assertEquals(NativeViewport.SKETCH_READY, sketchState());
        fact("revolve_after_edit", Arrays.toString(revolveState()));
        assertTrue("Revolve is the commit over a revolved body: " + statusLine(), shown(R.id.revolve_sketch));
        press(R.id.revolve_sketch);
        assertEquals(NativeViewport.SKETCH_INACTIVE, sketchState());
        assertEquals(body, NativeViewport.sceneActiveBodyId());
        final double v1 = measure(body)[NativeViewport.CAD_MEASURE_VOLUME];
        fact("revolve_volumes", v0 + " -> " + v1);
        assertEquals("the volume follows the width (centroid radius unchanged)",
                v0 * width / square[3], v1, v1 * 1e-6);
        capture("24_revolve_regenerated");
    }

    // =======================================================================
    // DEV-DR-12 — regression bundle
    // =======================================================================

    @Test
    public void devDr12_a_construction_diagonal_leaves_twenty_fill_cells_that_real_taps_select_and_extrude() {
        testName = "dr12";
        newCadSketch();
        final int cols = 5;
        final int rows = 4;
        final double cell = 0.4;
        final double hu = 0.5 * cols * cell;
        final double hv = 0.5 * rows * cell;
        final double[] frame = drawRectangle(-hu, -hv, hu, hv);
        assertEquals(0.0, frame[1], EXACT);
        assertEquals(0.0, frame[2], EXACT);
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        for (int k = 1; k < cols; k++) placeLine(-hu + k * cell, -hv, -hu + k * cell, hv);
        for (int k = 1; k < rows; k++) placeLine(-hu, -hv + k * cell, hu, -hv + k * cell);
        placeLine(-hu - 0.2, -hv - 0.2, hu + 0.2, hv + 0.2);
        openPalette();
        pressAction(R.id.sketch_action_construction);
        press(R.id.finish_sketch);
        assertEquals(NativeViewport.SKETCH_READY, sketchState());
        final int cells = cols * rows;
        assertEquals("the construction diagonal splits no cell", cells, NativeViewport.sketchProfiles(null));
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(NativeViewport.debugSetCameraPose(0.35f, 0.9f, 8.0f));
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        for (int k = 0; k < cells; k++) {
            final double u = -hu + (k % cols + 0.5) * cell;
            final double v = -hv + (k / cols + 0.5) * cell;
            final int beforeCount = selectedFaces();
            final float[] at = cellPoint(u, v, 0.3 * cell);
            assertNotNull("cell " + k + " is reachable: " + lastBlock, at);
            realGesture(new float[][]{at});
            assertEquals("cell " + k + " joined", beforeCount + 1, selectedFaces());
        }
        assertTrue("more than eighteen faces", selectedFaces() > 18);
        capture("25_twenty_faces");
        final double depth = sketchStateValue(NativeViewport.SKETCH_EXTRUDE_DEPTH);
        assertTrue("Extrude is drawn over a valid candidate", shown(R.id.extrude_sketch));
        press(R.id.extrude_sketch);
        final long body = NativeViewport.sceneActiveBodyId();
        final double block = cells * cell * cell * depth;
        assertEquals(block, measure(body)[NativeViewport.CAD_MEASURE_VOLUME], 1e-6 * block);
    }

    // =======================================================================
    // DEV-DR-13 — a dimension label's touch box
    // =======================================================================

    /**
     * A label owns taps on ITSELF and nothing else. Two labels that used to
     * reach back over the stroke they measure -- a rectangle's Height, standing
     * off a VERTICAL edge, and a circle's Diameter, off the curve at 45 degrees
     * -- are tapped as a user taps: on the label (it opens), just inside its
     * near side (still the label's), on the very point of the stroke it
     * measures (the drawing's: the entity is selected), and just beyond its far
     * side (the drawing's: empty space clears the selection). Every point's
     * owner is asserted BEFORE the touch, from the window's own hit order.
     */
    @Test
    public void devDr13_a_label_owns_its_own_box_and_the_stroke_it_measures_stays_tappable() {
        testName = "dr13";
        newCadSketch();
        final double[] rect = drawRectangle(-2.75 * D, -0.5 * D, -1.25 * D, 0.5 * D);
        final long rectangle = (long) rect[0];
        openPalette();
        pressAction(R.id.sketch_action_dimension);
        press(R.id.sketch_dimension_kind_height);
        press(R.id.sketch_modify_done);
        final double[] circle = drawCircle(-2.25 * D, -2.5 * D, 0.5 * D);
        final long disk = (long) circle[0];
        openPalette();
        pressAction(R.id.sketch_action_dimension);
        press(R.id.sketch_dimension_kind_diameter);
        press(R.id.sketch_modify_done);
        final double[][] dims = dimensions();
        final long heightId = dimensionId(dims, NativeViewport.DIM_RECTANGLE_HEIGHT);
        final long diameterId = dimensionId(dims, NativeViewport.DIM_CIRCLE_DIAMETER);
        // All: the labels stand with nothing selected, so a tap that clears the
        // selection does not also clear the label it is measured against.
        setVisibility(R.id.sketch_dimensions_all);
        clearSelectionByTap(-3.0 * D, -1.25 * D);
        assertCollisionPolicy("all", 2);
        assertLabelsAttached("all");
        capture("26_label_boxes");

        labelOwnsOnlyItsBox("height", heightId, rectangle);
        labelOwnsOnlyItsBox("diameter", diameterId, disk);
    }

    /** The four taps of DEV-DR-13 against one label and the entity it measures. */
    private void labelOwnsOnlyItsBox(String name, long dimensionId, long entity) {
        final float[] box = layoutBox(dimensionId);
        assertNotNull(name + ": the label stands", box);
        final float[] attach = attachPoint(dimensionId);
        final float density = (float) (dpM * pixelsPerMeter());
        fact(name + ".box", Arrays.toString(box) + " attach " + Arrays.toString(attach));
        assertFalse(name + ": the label's box does not cover the point of the stroke it measures",
                attach[0] >= box[0] && attach[0] <= box[2] && attach[1] >= box[1] && attach[1] <= box[3]);
        final float cx = 0.5f * (box[0] + box[2]);
        final float cy = 0.5f * (box[1] + box[3]);
        // The unit direction the label stands off in, from the stroke.
        final float dx = cx - attach[0];
        final float dy = cy - attach[1];
        final float r = (float) Math.hypot(dx, dy);
        final float ux = dx / r;
        final float uy = dy / r;
        // The box's reach along that direction, each side of its centre.
        final float reach = 0.5f * (Math.abs(ux) * (box[2] - box[0]) + Math.abs(uy) * (box[3] - box[1]));

        // 1. On the label: it opens.
        final float[] centre = {cx, cy};
        assertEquals(name + ": the label owns its centre", R.id.sketch_dimension_label_item, ownerAt(centre));
        realGesture(new float[][]{centre});
        assertEquals(name + ": a tap on the label opens it", dimensionId, editingDimension());
        closeLabelEditor();

        // 2. The label's own point nearest the stroke, 4 dp in: still the label's.
        final float inset = 4f * density;
        final float[] inside = {Math.max(box[0] + inset, Math.min(attach[0], box[2] - inset)),
                Math.max(box[1] + inset, Math.min(attach[1], box[3] - inset))};
        assertEquals(name + ": its point nearest the stroke, 4 dp in, is the label's",
                R.id.sketch_dimension_label_item, ownerAt(inside));
        realGesture(new float[][]{inside});
        assertEquals(name + ": and opens it", dimensionId, editingDimension());
        closeLabelEditor();

        // 3. On the stroke it measures: the drawing's.
        clearSelectionByTap(-3.0 * D, -1.25 * D);
        assertEquals(name + ": the stroke beside the label is the viewport's", R.id.viewport_surface,
                ownerAt(attach));
        realGesture(new float[][]{attach});
        assertArrayEquals(name + ": the tap on the stroke selected the entity", new long[]{entity}, selection());
        assertEquals(name + ": and opened no label", 0L, editingDimension());

        // 4. Just beyond its far side, on empty drawing: the drawing's.
        final float out = reach + 6f * density;
        final float[] beyond = {cx + ux * out, cy + uy * out};
        assertEquals(name + ": 6 dp beyond its far side is the viewport's", R.id.viewport_surface,
                ownerAt(beyond));
        realGesture(new float[][]{beyond});
        assertEquals(name + ": empty drawing beyond the label clears the selection", 0, selection().length);
        assertEquals(name + ": and opened no label", 0L, editingDimension());
    }

    /** {left, top, right, bottom} of a standing label, viewport px, or null. */
    private float[] layoutBox(long dimensionId) {
        final float[] rows = labelLayout();
        final int s = SketchDimensionLabelsView.LAYOUT_STRIDE;
        for (int i = 0; i < rows.length / s; i++) {
            if ((long) rows[i * s] == dimensionId && rows[i * s + 2] != 0f) {
                return new float[]{rows[i * s + 4], rows[i * s + 5], rows[i * s + 6], rows[i * s + 7]};
            }
        }
        return null;
    }

    /** Native's attach point of a label, viewport px. */
    private static float[] attachPoint(long dimensionId) {
        final double[] out = nativeLabels();
        for (int o = 0; o < out.length; o += NativeViewport.SKETCH_LABEL_STRIDE) {
            if ((long) out[o + NativeViewport.SKETCH_LABEL_ID] == dimensionId) {
                return new float[]{(float) out[o + NativeViewport.SKETCH_LABEL_ATTACH_X],
                        (float) out[o + NativeViewport.SKETCH_LABEL_ATTACH_Y]};
            }
        }
        fail("dimension " + dimensionId + " has a label");
        return null;
    }

    /** The id of the view a finger at this viewport pixel lands on, by the window's own hit order. */
    private int ownerAt(float[] at) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final int[] vp = new int[2];
            viewport.getLocationInWindow(vp);
            final View owner = clickableAt(activity.getWindow().getDecorView(), at[0] + vp[0], at[1] + vp[1]);
            return owner == null ? View.NO_ID : owner.getId();
        });
    }

    private long editingDimension() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.sketchDimensionLabels().editingDimensionId());
    }

    // -----------------------------------------------------------------------
    // Scenes
    // -----------------------------------------------------------------------

    private void newCadSketch() {
        press(R.id.home_new_project);
        press(R.id.new_project_cad);
        assertEquals("New CAD lands in a sketch", NativeViewport.SKETCH_EDITING, sketchState());
        settleLayout();
        final float[] o = new float[2];
        final float[] x = new float[2];
        assertTrue(NativeViewport.sketchScreenPoint(0.0, 0.0, o));
        assertTrue(NativeViewport.sketchScreenPoint(1.0, 0.0, x));
        final float density = onWorkspace(rule.getScenario(),
                (activity, workspace) -> activity.getResources().getDisplayMetrics().density);
        final double pixelsPerMeter = Math.hypot(x[0] - o[0], x[1] - o[1]);
        dpM = density / pixelsPerMeter;
        D = round3(60.0 * dpM);
        fact("scale", "density=" + density + " px/m=" + pixelsPerMeter + " D=" + D);
        assertTrue("the sketch view has a usable scale", D > 0.0);
        assertTrue("the Modify entry stands in the drawn sketch", shown(R.id.sketch_modify_toggle));
    }

    private static double round3(double value) {
        return Math.round(value * 1000.0) / 1000.0;
    }

    /** A line by a real window drag, then typed to its exact endpoints. */
    private long placeLine(double u0, double v0, double u1, double v1) {
        final int before = sketchEntityCount();
        realDrag(u0, v0, u1, v1);
        assertEquals("one line placed", before + 1, sketchEntityCount());
        final double[] drawn = new double[NativeViewport.SKETCH_ENTITY_SIZE];
        assertTrue("the new line is selected", NativeViewport.sketchSelectedEntity(drawn));
        assertEquals(NativeViewport.SKETCH_ENTITY_KIND_LINE, (int) drawn[NativeViewport.SKETCH_ENTITY_KIND]);
        final long id = (long) drawn[NativeViewport.SKETCH_ENTITY_ID];
        assertEquals(NativeViewport.CAD_OK, applyOnUi(() -> NativeViewport.sketchApplyLine(id, u0, v0, u1, v1)));
        return id;
    }

    /**
     * A rectangle by a real drag, its size typed exact; returns
     * {id, cu, cv, width, height}. The centre is where the drag put it.
     */
    private double[] drawRectangle(double u0, double v0, double u1, double v1) {
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        realDrag(u0, v0, u1, v1);
        final double[] drawn = new double[NativeViewport.SKETCH_ENTITY_SIZE];
        assertTrue(NativeViewport.sketchSelectedEntity(drawn));
        assertEquals(NativeViewport.SKETCH_ENTITY_KIND_RECTANGLE, (int) drawn[NativeViewport.SKETCH_ENTITY_KIND]);
        final long id = (long) drawn[NativeViewport.SKETCH_ENTITY_ID];
        final double w = Math.abs(u1 - u0);
        final double h = Math.abs(v1 - v0);
        assertEquals(NativeViewport.CAD_OK, applyOnUi(() -> NativeViewport.sketchApplyRectangle(id, w, h)));
        final double[] e = entity(id);
        // Re-centre exactly where the scene asked, by the line apply's sibling:
        // a rectangle's centre is authored truth like its size.
        final double cu = 0.5 * (u0 + u1);
        final double cv = 0.5 * (v0 + v1);
        if (Math.abs(e[2] - cu) > EXACT || Math.abs(e[3] - cv) > EXACT) {
            fact("rect.recentre", Arrays.toString(e) + " wanted " + cu + "," + cv);
        }
        return new double[]{id, e[2], e[3], e[4], e[5]};
    }

    /** A circle by a real drag from its centre, radius typed exact; {id, cu, cv, r}. */
    private double[] drawCircle(double cu, double cv, double radius) {
        selectTool(rule.getScenario(), R.id.tool_rail_circle);
        realDrag(cu, cv, cu + radius, cv);
        final double[] drawn = new double[NativeViewport.SKETCH_ENTITY_SIZE];
        assertTrue(NativeViewport.sketchSelectedEntity(drawn));
        assertEquals(NativeViewport.SKETCH_ENTITY_KIND_CIRCLE, (int) drawn[NativeViewport.SKETCH_ENTITY_KIND]);
        final long id = (long) drawn[NativeViewport.SKETCH_ENTITY_ID];
        assertEquals(NativeViewport.CAD_OK, applyOnUi(() -> NativeViewport.sketchApplyCircle(id, radius)));
        final double[] e = entity(id);
        return new double[]{id, e[2], e[3], e[4]};
    }

    /** An arc by a real drag start → end and a real tap on its through point. */
    private long drawArc(double su, double sv, double eu, double ev, double mu, double mv) {
        selectTool(rule.getScenario(), R.id.tool_rail_arc);
        final int before = sketchEntityCount();
        realDrag(su, sv, eu, ev);
        tapSketchReal(mu, mv);
        assertEquals("one arc placed", before + 1, sketchEntityCount());
        return newestEntity(NativeViewport.SKETCH_ENTITY_KIND_ARC, 0);
    }

    private void clearSketch() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            for (long id : entityIds()) {
                NativeViewport.sketchSelectEntity(id);
                assertEquals(NativeViewport.CAD_OK, NativeViewport.sketchDeleteSelected());
            }
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        assertEquals(0, sketchEntityCount());
        selectTool(rule.getScenario(), R.id.tool_rail_line);
    }

    /**
     * One snap case: the Line tool, a real drag from S to T (offset by a few
     * dp), the kind the end landed on and its exact coordinates. The new line
     * is deleted again so the next case sees only its own references.
     */
    private void snapCase(String name, double su, double sv, double tu, double tv, float offXdp,
                          float offYdp, int expectedKind, double expectU, double expectV) {
        // The DOWN must be free for a finger. The target need only be on the
        // viewport: a line being dragged out belongs to the view that took its
        // Down, so a label standing over the target cannot take the gesture --
        // exactly as on the device, where the reference line drawn last is
        // still selected and its length label stands in the drawing.
        final float[] from = sketchPoint(su, sv);
        assertNotNull(name + ": the start is free for a finger " + lastBlock, from);
        final float[] to = sketchPointOnViewport(tu, tv);
        final float density = (float) (dpM * pixelsPerMeter());
        to[0] += offXdp * density;
        to[1] += offYdp * density;
        final int before = sketchEntityCount();
        realGesture(path(from, to, 8));
        assertEquals(name + ": one line drawn", before + 1, sketchEntityCount());
        final double[] drawn = new double[NativeViewport.SKETCH_ENTITY_SIZE];
        assertTrue(NativeViewport.sketchSelectedEntity(drawn));
        final long id = (long) drawn[NativeViewport.SKETCH_ENTITY_ID];
        final double[] e = entity(id);
        final double[] state = draft();
        final int kind = (int) state[NativeViewport.SKETCH_DRAFT_LAST_SNAP];
        fact("snap." + name, "kind=" + kind + " end=(" + e[4] + "," + e[5] + ") guides h="
                + state[NativeViewport.SKETCH_DRAFT_GUIDE_H] + " v=" + state[NativeViewport.SKETCH_DRAFT_GUIDE_V]);
        assertEquals(name + ": the snap kind", expectedKind, kind);
        if (!Double.isNaN(expectU)) assertEquals(name + ": exact u", expectU, e[4], EXACT);
        if (!Double.isNaN(expectV)) assertEquals(name + ": exact v", expectV, e[5], EXACT);
        capture("snap_" + name);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.sketchSelectEntity(id);
            NativeViewport.sketchDeleteSelected();
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    private double pixelsPerMeter() {
        final float[] o = new float[2];
        final float[] x = new float[2];
        assertTrue(NativeViewport.sketchScreenPoint(0.0, 0.0, o));
        assertTrue(NativeViewport.sketchScreenPoint(1.0, 0.0, x));
        return Math.hypot(x[0] - o[0], x[1] - o[1]);
    }

    /** Finish → count the areas → Back to Sketch. */
    private int readyAreas() {
        press(R.id.finish_sketch);
        assertEquals("Finish succeeds: " + NativeViewport.sketchLastStatus() + " '" + statusLine() + "'",
                NativeViewport.SKETCH_READY, sketchState());
        final int areas = (int) sketchStateValue(NativeViewport.SKETCH_PROFILE_COUNT);
        press(R.id.back_to_sketch);
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
        return areas;
    }

    /**
     * Opens an extruded CAD body's sketch for editing through the canvas Edit
     * Sketch control standing on it -- which must be there: no native door.
     */
    private void editSketchFromCanvas(long body) {
        refresh();
        assertEquals(body, NativeViewport.sceneActiveBodyId());
        assertTrue("the canvas Edit Sketch control stands on the extruded body",
                shown(R.id.cad_canvas_edit_sketch));
        press(R.id.cad_canvas_edit_sketch);
        reachEditing();
    }

    /**
     * Opens the active CAD body's sketch for editing through the precision
     * surface: its toggle, then Edit Sketch -- both pressed as a user presses.
     */
    private void editSketchFromInspector() {
        refresh();
        assertEquals("no sketch is open yet", NativeViewport.SKETCH_INACTIVE, sketchState());
        press(R.id.precision_toggle);
        assertTrue("the precision surface offers Edit Sketch", shown(R.id.edit_cad_sketch));
        press(R.id.edit_cad_sketch);
        reachEditing();
    }

    /** From an opened edit, the drawn sketch: Back to Sketch when it opened staged in Ready. */
    private void reachEditing() {
        if (sketchState() == NativeViewport.SKETCH_READY) {
            press(R.id.back_to_sketch);
        }
        assertEquals("the retained sketch is open for drawing", NativeViewport.SKETCH_EDITING, sketchState());
    }

    // -----------------------------------------------------------------------
    // The palette and the labels
    // -----------------------------------------------------------------------

    private void openPalette() {
        final boolean open = onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.sketchModify().paletteOpen());
        if (!open) {
            press(R.id.sketch_modify_toggle);
        }
        assertTrue("the palette is open", onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.sketchModify().paletteOpen()));
    }

    private void closePalette() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.sketchModify().closePalette();
            return null;
        });
        settleLayout();
    }

    private void pressAction(int id) {
        openPalette();
        press(id);
    }

    private void setVisibility(int chipId) {
        openPalette();
        press(chipId);
        closePalette();
    }

    /** The palette and the mode capsule stand in the trailing column; nothing spans the bottom. */
    private void assertNoBottomToolbar() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View modify = workspace.sketchModify();
            final View navigator = workspace.sketchNavigator();
            final Rect m = boundsOf(modify);
            final Rect n = boundsOf(navigator);
            final View root = activity.getWindow().getDecorView();
            fact("layout.modify", m.toShortString() + " navigator " + n.toShortString()
                    + " window " + root.getWidth() + "x" + root.getHeight());
            assertTrue("the Modify column stands under the navigator", m.top >= n.bottom - 1);
            assertTrue("it is a corner column, not a bar", m.width() < root.getWidth() * 0.75f);
            assertFalse("it never overlaps the navigator", Rect.intersects(m, n));
            final View rail = workspace.findViewById(R.id.tool_rail_select);
            if (rail != null && rail.isShown()) {
                assertFalse("it never stands on the Tool Rail", Rect.intersects(boundsOf(rail), boundsOf(
                        workspace.sketchModify().findViewById(R.id.sketch_modify_toggle))));
            }
            return null;
        });
    }

    private static Rect boundsOf(View view) {
        final int[] at = new int[2];
        view.getLocationInWindow(at);
        return new Rect(at[0], at[1], at[0] + view.getWidth(), at[1] + view.getHeight());
    }

    private String chipText(int id) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View view = workspace.findViewById(id);
            assertNotNull("control " + id, view);
            return ((TextView) view).getText().toString();
        });
    }

    private int nativeLabelCount() {
        final double[] out = new double[NativeViewport.SKETCH_LABEL_MAX * NativeViewport.SKETCH_LABEL_STRIDE];
        return NativeViewport.sketchDimensionLabels(out);
    }

    private int visibleLabelCount() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.sketchDimensionLabels().visibleLabelCount());
    }

    private String labelText(long dimensionId) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final TextView label = workspace.sketchDimensionLabels().labelFor(dimensionId);
            assertNotNull("the label of dimension " + dimensionId + " is drawn", label);
            return label.getText().toString();
        });
    }

    /** Taps a label as a user does; returns the value field (shown or not). */
    private View openLabelEditor(long dimensionId) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final TextView label = workspace.sketchDimensionLabels().labelFor(dimensionId);
            assertNotNull("the label of dimension " + dimensionId + " is drawn", label);
            assertTrue("and meets the 48 dp floor", label.getHeight()
                    >= Math.round(48f * activity.getResources().getDisplayMetrics().density) - 1);
            label.performClick();
            return null;
        });
        settleLayout();
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(dimensionId, workspace.sketchDimensionLabels().editingDimensionId());
            return workspace.findViewById(R.id.field_sketch_dimension_value);
        });
    }

    private void closeLabelEditor() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.sketchDimensionLabels().closeEditor();
            return null;
        });
        settleLayout();
    }

    /** Opens a driving label, types, Apply — the user's exact path. */
    private void typeOnLabel(long dimensionId, String text) {
        final View field = openLabelEditor(dimensionId);
        assertTrue("a driving label opens its value", field.isShown());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            ((EditText) field).setText(text);
            workspace.findViewById(R.id.apply_sketch_dimension_value).performClick();
            return null;
        });
        settleLayout();
        assertEquals("the typed value landed: " + statusLine(), NativeViewport.CAD_OK,
                NativeViewport.sketchLastStatus());
    }

    private void typeOffset(String text) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final EditText field = workspace.findViewById(R.id.field_sketch_offset_distance);
            assertNotNull(field);
            assertTrue("the exact offset field is drawn", field.isShown());
            field.setText(text);
            field.onEditorAction(EditorInfo.IME_ACTION_DONE);
            return null;
        });
        settleLayout();
    }

    /** No two drawn labels overlap: a collision hides, it never stacks. */
    private void assertNoChipOverlap() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final ViewGroup labels = workspace.sketchDimensionLabels();
            final List<Rect> boxes = new ArrayList<>();
            for (int i = 0; i < labels.getChildCount(); i++) {
                final View chip = labels.getChildAt(i);
                if (chip.getId() == R.id.sketch_dimension_label_item && chip.getVisibility() == View.VISIBLE) {
                    final Rect box = boundsOf(chip);
                    box.inset(2, 2);
                    for (Rect other : boxes) {
                        assertFalse("labels overlap: " + box + " " + other, Rect.intersects(box, other));
                    }
                    boxes.add(box);
                }
            }
            return null;
        });
    }

    /** The label view's last layout rows (SketchDimensionLabelsView.LAYOUT_STRIDE). */
    private float[] labelLayout() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.sketchDimensionLabels().labelLayout());
    }

    /** Native's label rows, stride SKETCH_LABEL_STRIDE. */
    private static double[] nativeLabels() {
        final double[] out = new double[NativeViewport.SKETCH_LABEL_MAX * NativeViewport.SKETCH_LABEL_STRIDE];
        final int count = NativeViewport.sketchDimensionLabels(out);
        return Arrays.copyOf(out, count * NativeViewport.SKETCH_LABEL_STRIDE);
    }

    private static boolean boxesMeet(float[] rows, int a, int b) {
        final int s = SketchDimensionLabelsView.LAYOUT_STRIDE;
        return rows[a * s + 4] < rows[b * s + 6] && rows[b * s + 4] < rows[a * s + 6]
                && rows[a * s + 5] < rows[b * s + 7] && rows[b * s + 5] < rows[a * s + 7];
    }

    /**
     * The collision policy, as a statement about EVERY label rather than a
     * count: a label stands unless it has a stated reason not to. A label is
     * hidden as a collision only when a STANDING label of higher priority
     * claims an overlapping box; one hidden as off the viewport really has a
     * box that leaves it; and no two standing labels meet. So a label whose
     * box conflicts with nothing never disappears, and an unrelated one cannot.
     */
    private void assertCollisionPolicy(String when, int expectedRows) {
        final float[] rows = labelLayout();
        final int s = SketchDimensionLabelsView.LAYOUT_STRIDE;
        final int n = rows.length / s;
        assertEquals(when + ": one layout row per native label", expectedRows, n);
        final int[] size = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            return new int[]{viewport.getWidth(), viewport.getHeight()};
        });
        final StringBuilder record = new StringBuilder();
        int standing = 0;
        for (int i = 0; i < n; i++) {
            final long id = (long) rows[i * s];
            final boolean shown = rows[i * s + 2] != 0f;
            final int why = (int) rows[i * s + 3];
            record.append(id).append(shown ? ":shown " : ":hidden" + why + " ");
            final boolean inside = rows[i * s + 4] >= 0f && rows[i * s + 5] >= 0f
                    && rows[i * s + 6] <= size[0] && rows[i * s + 7] <= size[1];
            if (shown) {
                standing++;
                assertTrue(when + ": standing label " + id + " lies wholly on the viewport", inside);
                for (int j = 0; j < n; j++) {
                    if (j != i && rows[j * s + 2] != 0f) {
                        assertFalse(when + ": standing labels " + id + " and " + (long) rows[j * s]
                                + " never meet", boxesMeet(rows, i, j));
                    }
                }
                continue;
            }
            if (why == SketchDimensionLabelsView.HIDDEN_OFF_VIEWPORT) {
                assertFalse(when + ": label " + id + " hidden as off the viewport really leaves it", inside);
            } else if (why == SketchDimensionLabelsView.HIDDEN_COLLISION) {
                boolean claimed = false;
                for (int j = 0; j < n; j++) {
                    claimed |= j != i && rows[j * s + 2] != 0f && rows[j * s + 1] >= rows[i * s + 1]
                            && boxesMeet(rows, i, j);
                }
                assertTrue(when + ": label " + id + " is hidden only by a standing label of higher "
                        + "priority whose box it meets", claimed);
            } else {
                assertEquals(when + ": label " + id + " is hidden for a stated reason",
                        SketchDimensionLabelsView.HIDDEN_DOES_NOT_PROJECT, why);
            }
        }
        fact("policy." + when, standing + "/" + n + " " + record.toString().trim());
        assertTrue(when + ": labels stand", standing > 0);
    }

    /**
     * Every drawn label is attached to what it measures: its box centre lies
     * on the ray from native's attach point (on the geometry) through native's
     * anchor, no nearer than the anchor, and the box clears the attach point --
     * the label owns taps on itself and never on its own stroke.
     */
    private void assertLabelsAttached(String when) {
        final double[] out = nativeLabels();
        final int count = out.length / NativeViewport.SKETCH_LABEL_STRIDE;
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final int[] vp = new int[2];
            viewport.getLocationInWindow(vp);
            int checked = 0;
            for (int i = 0; i < count; i++) {
                final int o = i * NativeViewport.SKETCH_LABEL_STRIDE;
                final long id = (long) out[o + NativeViewport.SKETCH_LABEL_ID];
                final TextView chip = workspace.sketchDimensionLabels().labelFor(id);
                if (chip == null) continue;
                final float ax = (float) out[o + NativeViewport.SKETCH_LABEL_X];
                final float ay = (float) out[o + NativeViewport.SKETCH_LABEL_Y];
                final float tx = (float) out[o + NativeViewport.SKETCH_LABEL_ATTACH_X];
                final float ty = (float) out[o + NativeViewport.SKETCH_LABEL_ATTACH_Y];
                final Rect box = boundsOf(chip);
                box.offset(-vp[0], -vp[1]);
                final float cx = box.exactCenterX();
                final float cy = box.exactCenterY();
                final float rx = ax - tx;
                final float ry = ay - ty;
                final float r = (float) Math.hypot(rx, ry);
                if (r > 0.5f) {
                    final float along = ((cx - tx) * rx + (cy - ty) * ry) / r;
                    final float off = Math.abs((cx - tx) * ry - (cy - ty) * rx) / r;
                    assertTrue(when + ": label " + id + " stands on its ray (off " + off + " px)", off <= 3f);
                    assertTrue(when + ": label " + id + " is never pulled nearer than its anchor",
                            along >= r - 3f);
                    assertFalse(when + ": label " + id + " " + box.toShortString()
                                    + " stands clear of the geometry it measures at (" + tx + ", " + ty + ")",
                            box.contains(Math.round(tx), Math.round(ty)));
                } else {
                    assertEquals(when + ": label " + id + " x", ax, cx, 3.0f);
                    assertEquals(when + ": label " + id + " y", ay, cy, 3.0f);
                }
                checked++;
            }
            fact("attached." + when, checked + "/" + count);
            assertTrue(when + ": some labels were checked", checked > 0);
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // Native truth
    // -----------------------------------------------------------------------

    private static double[] draft() {
        final double[] out = new double[NativeViewport.SKETCH_DRAFT_SIZE];
        NativeViewport.sketchDraftingState(out);
        return out;
    }

    private static double sketchStateValue(int slot) {
        final double[] state = new double[NativeViewport.SKETCH_STATE_SIZE];
        NativeViewport.sketchState(state);
        return state[slot];
    }

    private static long[] selection() {
        final long[] out = new long[64];
        final int n = NativeViewport.sketchSelection(out);
        return Arrays.copyOf(out, n);
    }

    private static long[] entityIds() {
        final long[] out = new long[1024];
        final int n = NativeViewport.sketchEntityIds(out);
        return Arrays.copyOf(out, n);
    }

    private static double[] entity(long id) {
        final double[] out = entityOrNull(id);
        assertNotNull("entity " + id + " exists", out);
        return out;
    }

    private static double[] entityOrNull(long id) {
        final double[] out = new double[NativeViewport.SKETCH_ENTITY_VALUES_SIZE];
        return NativeViewport.sketchEntityValues(id, out) ? out : null;
    }

    /** The newest entity of a kind other than {@code except}. */
    private static long newestEntity(int kind, long except) {
        long found = 0;
        for (long id : entityIds()) {
            final double[] e = entity(id);
            if ((int) e[1] == kind && id != except && id > found) found = id;
        }
        assertTrue("an entity of kind " + kind, found > 0);
        return found;
    }

    private static double[][] dimensions() {
        final double[] out = new double[512 * NativeViewport.SKETCH_DIM_STRIDE];
        final int n = NativeViewport.sketchDimensions(out);
        final double[][] rows = new double[n][];
        for (int i = 0; i < n; i++) {
            rows[i] = Arrays.copyOfRange(out, i * NativeViewport.SKETCH_DIM_STRIDE,
                    (i + 1) * NativeViewport.SKETCH_DIM_STRIDE);
        }
        return rows;
    }

    private static long dimensionId(double[][] dims, int kind) {
        for (double[] d : dims) {
            if ((int) d[NativeViewport.SKETCH_DIM_KIND] == kind) return (long) d[NativeViewport.SKETCH_DIM_ID];
        }
        throw new AssertionError("no dimension of kind " + kind + " in " + Arrays.deepToString(dims));
    }

    private static double[] dimension(double[][] dims, long id) {
        for (double[] d : dims) {
            if ((long) d[NativeViewport.SKETCH_DIM_ID] == id) return d;
        }
        throw new AssertionError("no dimension " + id);
    }

    private static double[] revolveState() {
        final double[] state = new double[NativeViewport.REVOLVE_STATE_SIZE];
        NativeViewport.cadRevolveToolState(state);
        return state;
    }

    private static double[] candidateMeasure() {
        final double[] out = new double[NativeViewport.CANDIDATE_MEASURE_SIZE];
        assertTrue("the candidate measures", NativeViewport.sketchCandidateMeasure(out));
        assertEquals(NativeViewport.CAD_OK, (int) out[NativeViewport.CANDIDATE_MEASURE_STATUS]);
        return out;
    }

    private static double[] measure(long body) {
        final double[] out = new double[NativeViewport.CAD_MEASURE_SIZE];
        assertTrue("the body measures as a CAD solid", NativeViewport.cadBodyMeasure(body, out));
        return out;
    }

    private static int selectedFaces() {
        final int count = NativeViewport.sketchProfiles(null);
        final long[] faces = new long[Math.max(count, 0)];
        NativeViewport.sketchProfiles(faces);
        final double[] info = new double[NativeViewport.SKETCH_REGION_INFO_SIZE];
        int selected = 0;
        for (long handle : faces) {
            assertTrue(NativeViewport.sketchProfileInfo(handle, info));
            if (info[NativeViewport.SKETCH_REGION_SELECTED] != 0.0) selected++;
        }
        return selected;
    }

    private static int cadbVersion(byte[] bytes) {
        for (int i = 28; i + 6 <= bytes.length; i++) {
            if (bytes[i] == 'C' && bytes[i + 1] == 'A' && bytes[i + 2] == 'D' && bytes[i + 3] == 'B') {
                return (bytes[i + 4] & 0xFF) | ((bytes[i + 5] & 0xFF) << 8);
            }
        }
        return -1;
    }

    private static byte[] readSlot(Context context) {
        try {
            return ProjectSlot.read(context);
        } catch (Exception error) {
            throw new AssertionError("the slot reads back: " + error);
        }
    }

    // -----------------------------------------------------------------------
    // Real window touches
    // -----------------------------------------------------------------------

    /** Why the last point asked of {@link #sketchPoint} was refused, for the failure message. */
    private String lastBlock = "";

    /**
     * The viewport pixel of a sketch point a finger can put a DOWN on: it
     * projects, stands on the viewport, and no clickable chrome (a label, the
     * Modify column, the rail) is over it. Null otherwise, with the reason in
     * {@link #lastBlock}.
     */
    private float[] sketchPoint(double u, double v) {
        final float[] at = new float[2];
        if (!NativeViewport.sketchScreenPoint(u, v, at)) {
            lastBlock = "(" + u + "," + v + ") does not project";
            return null;
        }
        return viewportPoint(at);
    }

    /**
     * The viewport pixel of a sketch point a gesture already in progress may
     * pass over or end on: it need only project onto the viewport. Chrome there
     * is irrelevant -- every later event of a gesture goes to the view that
     * took its DOWN, which is exactly how a finger dragging out a line over a
     * label behaves.
     */
    private float[] sketchPointOnViewport(double u, double v) {
        final float[] at = new float[2];
        assertTrue("(" + u + ", " + v + ") projects", NativeViewport.sketchScreenPoint(u, v, at));
        final boolean on = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            return at[0] >= 0 && at[1] >= 0 && at[0] < viewport.getWidth() && at[1] < viewport.getHeight();
        });
        assertTrue("(" + u + ", " + v + ") -> " + Arrays.toString(at) + " is on the viewport", on);
        return at;
    }

    /** A still real tap at a sketch point that must be free for a finger. */
    private void tapAt(double u, double v) {
        final float[] at = sketchPoint(u, v);
        assertNotNull("(" + u + ", " + v + ") is free for a tap: " + lastBlock, at);
        realGesture(new float[][]{at});
    }

    private void tapSketchReal(double u, double v) {
        tapAt(u, v);
    }

    /**
     * A still real tap ON an entity at ONE stated point, which must be free
     * for a finger; then, outside a modify mode, the entity must be selected,
     * and in Dimension mode it must be the target.
     */
    private void tapEntity(long id, double u, double v) {
        tapEntityAt(id, u, v);
        final double[] state = draft();
        final int mode = (int) state[NativeViewport.SKETCH_DRAFT_MODE];
        if (mode == NativeViewport.MODIFY_NONE) {
            boolean in = false;
            for (long s : selection()) in |= s == id;
            assertTrue("the tap chose entity " + id + " (selection " + Arrays.toString(selection())
                    + ", status '" + statusLine() + "')", in);
        } else if (mode == NativeViewport.MODIFY_DIMENSION) {
            assertEquals("the tap chose the dimension target", id,
                    (long) state[NativeViewport.SKETCH_DRAFT_DIM_TARGET_ENTITY]);
        }
    }

    /** A still real tap at ONE stated point on an entity, free for a finger; nothing asserted after. */
    private void tapEntityAt(long id, double u, double v) {
        final float[] at = sketchPoint(u, v);
        assertNotNull("entity " + id + " at (" + u + ", " + v + ") is free for a tap: " + lastBlock, at);
        realGesture(new float[][]{at});
        fact("tap." + id, Arrays.toString(at));
    }

    /** The Select tool, then a real tap on an entity; it alone is selected. */
    private void selectByTap(long id, double u, double v) {
        selectTool(rule.getScenario(), R.id.tool_rail_select);
        tapEntity(id, u, v);
        assertArrayEquals("the tap selected exactly entity " + id, new long[]{id}, selection());
    }

    /** The Select tool, then a real tap on EMPTY drawing: the selection clears, as a user clears it. */
    private void clearSelectionByTap(double u, double v) {
        selectTool(rule.getScenario(), R.id.tool_rail_select);
        tapAt(u, v);
        assertEquals("a tap on empty drawing clears the selection", 0, selection().length);
    }

    /** A real tap on the Revolve axis at ONE stated point; the axis is chosen. */
    private void tapRevolveAxis(long id, double u, double v) {
        final float[] at = sketchPoint(u, v);
        assertNotNull("the axis at (" + u + ", " + v + ") is free for a tap: " + lastBlock, at);
        realGesture(new float[][]{at});
        assertEquals("the tap chose the axis", id, (long) revolveState()[NativeViewport.REVOLVE_AXIS_ENTITY]);
    }

    /** A cell point a finger can reach in Ready: its centre, then four inset points. */
    private float[] cellPoint(double cu, double cv, double d) {
        final double[][] candidates = {{cu, cv}, {cu - d, cv - d}, {cu + d, cv - d},
                {cu - d, cv + d}, {cu + d, cv + d}};
        for (double[] p : candidates) {
            final float[] at = sketchPoint(p[0], p[1]);
            if (at != null && shaftDistance(at[0], at[1]) > 48.0f) return at;
        }
        return null;
    }

    private static float shaftDistance(float x, float y) {
        final double[] tool = new double[NativeViewport.CAD_EXTRUDE_SIZE];
        NativeViewport.cadExtrudeToolState(tool);
        if (tool[NativeViewport.CAD_EXTRUDE_ACTIVE] == 0.0 || tool[NativeViewport.CAD_EXTRUDE_ON_SCREEN] == 0.0) {
            return Float.POSITIVE_INFINITY;
        }
        final float tx = (float) tool[NativeViewport.CAD_EXTRUDE_TIP_X];
        final float ty = (float) tool[NativeViewport.CAD_EXTRUDE_TIP_Y];
        final float bx = 2.0f * (float) tool[NativeViewport.CAD_EXTRUDE_LABEL_X] - tx;
        final float by = 2.0f * (float) tool[NativeViewport.CAD_EXTRUDE_LABEL_Y] - ty;
        final float dx = tx - bx;
        final float dy = ty - by;
        final float len2 = dx * dx + dy * dy;
        float t = len2 > 0.0f ? ((x - bx) * dx + (y - by) * dy) / len2 : 0.0f;
        t = Math.max(0.0f, Math.min(1.0f, t));
        return (float) Math.hypot(x - (bx + dx * t), y - (by + dy * t));
    }

    /**
     * A real drag between two sketch points. The DOWN must be free for a
     * finger; the rest of the path need only be on the viewport, because the
     * view that takes the Down receives every later event of the gesture.
     */
    private void realDrag(double u0, double v0, double u1, double v1) {
        final float[] from = sketchPoint(u0, v0);
        assertNotNull("drag start (" + u0 + ", " + v0 + ") is free for a finger: " + lastBlock, from);
        final float[] to = sketchPointOnViewport(u1, v1);
        realGesture(path(from, to, 8));
    }

    private static float[][] path(float[] from, float[] to, int steps) {
        final float[][] out = new float[steps + 1][];
        for (int i = 0; i <= steps; i++) {
            final float t = (float) i / steps;
            out[i] = new float[]{from[0] + (to[0] - from[0]) * t, from[1] + (to[1] - from[1]) * t};
        }
        return out;
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
            return "under " + owner.getClass().getSimpleName() + " "
                    + (owner.getId() != View.NO_ID ? activity.getResources().getResourceEntryName(owner.getId())
                    : "(no id)");
        });
        lastBlock = block == null ? "" : "[" + at[0] + "," + at[1] + " " + block + "]";
        return block == null ? at : null;
    }

    private static View clickableAt(View view, float wx, float wy) {
        if (view.getVisibility() != View.VISIBLE) {
            return null;
        }
        final int[] at = new int[2];
        view.getLocationInWindow(at);
        if (wx < at[0] || wy < at[1] || wx >= at[0] + view.getWidth() || wy >= at[1] + view.getHeight()) {
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
                dispatch(root, down, down + 16L * i, MotionEvent.ACTION_MOVE, path[i][0] + ox, path[i][1] + oy);
            }
            final float[] last = path[path.length - 1];
            dispatch(root, down, down + 16L * path.length + 24L, MotionEvent.ACTION_UP, last[0] + ox, last[1] + oy);
            return null;
        });
        settleLayout();
    }

    private static void dispatch(View root, long downTime, long eventTime, int action, float x, float y) {
        final MotionEvent event = MotionEvent.obtain(downTime, eventTime, action, x, y, 0);
        event.setSource(InputDevice.SOURCE_TOUCHSCREEN);
        root.dispatchTouchEvent(event);
        event.recycle();
    }

    /** A real two-finger pinch about the viewport centre-left, through the window. */
    private void pinch(float factor) {
        twoFinger(0f, 0f, factor);
    }

    private void twoFingerPan(float dxDp, float dyDp) {
        twoFinger(dxDp, dyDp, 1.0f);
    }

    private void twoFinger(float dxDp, float dyDp, float factor) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final float density = activity.getResources().getDisplayMetrics().density;
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final View root = activity.getWindow().getDecorView();
            final int[] vp = new int[2];
            viewport.getLocationInWindow(vp);
            final float cx = vp[0] + viewport.getWidth() * 0.35f;
            final float cy = vp[1] + viewport.getHeight() * 0.5f;
            final float half = 60f * density;
            final long down = SystemClock.uptimeMillis();
            final int steps = 8;
            final MotionEvent.PointerProperties[] props = new MotionEvent.PointerProperties[2];
            for (int i = 0; i < 2; i++) {
                props[i] = new MotionEvent.PointerProperties();
                props[i].id = i;
                props[i].toolType = MotionEvent.TOOL_TYPE_FINGER;
            }
            final MotionEvent.PointerCoords[] coords = new MotionEvent.PointerCoords[2];
            for (int step = 0; step <= steps; step++) {
                final float t = (float) step / steps;
                final float spread = half * (1.0f + (factor - 1.0f) * t);
                final float ox = dxDp * density * t;
                final float oy = dyDp * density * t;
                for (int i = 0; i < 2; i++) {
                    coords[i] = new MotionEvent.PointerCoords();
                    coords[i].x = cx + ox + (i == 0 ? -spread : spread);
                    coords[i].y = cy + oy;
                    coords[i].pressure = 1f;
                    coords[i].size = 1f;
                }
                final long time = down + 16L * step;
                if (step == 0) {
                    send(root, down, time, MotionEvent.ACTION_DOWN, 1, props, coords);
                    send(root, down, time, MotionEvent.ACTION_POINTER_DOWN
                            | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT), 2, props, coords);
                } else {
                    send(root, down, time, MotionEvent.ACTION_MOVE, 2, props, coords);
                }
            }
            final long end = down + 16L * (steps + 1);
            send(root, down, end, MotionEvent.ACTION_POINTER_UP
                    | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT), 2, props, coords);
            send(root, down, end + 8L, MotionEvent.ACTION_UP, 1, props, coords);
            return null;
        });
        settleLayout();
    }

    private static void send(View root, long downTime, long time, int action, int count,
                             MotionEvent.PointerProperties[] props, MotionEvent.PointerCoords[] coords) {
        final MotionEvent event = MotionEvent.obtain(downTime, time, action, count, props, coords, 0, 0,
                1f, 1f, 0, 0, InputDevice.SOURCE_TOUCHSCREEN, 0);
        root.dispatchTouchEvent(event);
        event.recycle();
    }

    // -----------------------------------------------------------------------
    // Plumbing
    // -----------------------------------------------------------------------

    /** Selects one entity, so the Selected visibility shows exactly its labels. */
    private void focusEntity(long id) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(NativeViewport.sketchSelectEntity(id));
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    private void refresh() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    private void press(final int id) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View control = workspace.findViewById(id);
            assertNotNull("control " + id + " must exist", control);
            assertTrue("control " + activity.getResources().getResourceEntryName(id) + " must be on screen",
                    control.isShown());
            control.performClick();
            return null;
        });
        settleLayout();
    }

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
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.closePrecision(workspace);
            return null;
        });
        settleLayout();
    }

    private boolean shown(final int id) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View control = workspace.findViewById(id);
            return control != null && control.isShown();
        });
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

    private String string(int id, Object... args) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> activity.getString(id, args));
    }

    private String statusLine() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View message = workspace.findViewById(R.id.status_message);
            return message instanceof TextView && message.isShown()
                    ? ((TextView) message).getText().toString() : "";
        });
    }

    // -----------------------------------------------------------------------
    // Evidence
    // -----------------------------------------------------------------------

    private void capture(String name) {
        final long start = NativeViewport.debugRendererFramesPresented();
        final long began = SystemClock.uptimeMillis();
        long seen = 0;
        while (SystemClock.uptimeMillis() - began < 15000L && seen < 6) {
            final long now = NativeViewport.debugRendererFramesPresented();
            seen = now >= start ? now - start : now;
            SystemClock.sleep(50);
        }
        final Bitmap frame = InstrumentationRegistry.getInstrumentation().getUiAutomation().takeScreenshot();
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
        final String line = testName + "." + key + "=" + value;
        facts.add(line);
        Log.i(TAG, "CADDR_DEVICE " + line);
    }

    private void writeFacts() {
        final File file = new File(outDir, "facts-" + testName + "-" + System.nanoTime() + ".txt");
        try (PrintWriter out = new PrintWriter(file, "UTF-8")) {
            for (String line : facts) {
                out.println(line);
            }
        } catch (IOException error) {
            Log.w(TAG, "CADDR_DEVICE facts not written: " + error);
        }
    }
}
