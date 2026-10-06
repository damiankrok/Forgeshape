package com.forgeshape.app;

import static com.forgeshape.app.SketchTestSupport.dragSketch;
import static com.forgeshape.app.SketchTestSupport.selectTool;
import static com.forgeshape.app.SketchTestSupport.sketchEntityCount;
import static com.forgeshape.app.SketchTestSupport.sketchState;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.view.View;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;

/**
 * Surface on the device ({@code MODELING-FOUNDATIONS-R1} C), on the OWNER's own
 * path and by real window touches: Home → New Project → Surface, a sketch drawn
 * with the ordinary sketch tools, and Finish Sketch answered on the Surface
 * surface with a Patch, an Extruded, a Revolved or a Lofted surface; then Trim,
 * Stitch and Thicken; an earlier feature edited through the History with its
 * downstream features rebuilt; and save and reopen.
 *
 * <p>Asserted from native truth -- the feature list, the derived patches, open
 * edges, stitches and solid, the mesh digest, the encoded project -- and from
 * which controls are actually drawn. Exact sizes are typed through the sketch's
 * own exact-value entry after the real drag, so the geometry is what the
 * assertions name. Screenshots are evidence only.
 */
@RunWith(AndroidJUnit4.class)
public final class SurfaceOwnerTest {

    private static final double TOLERANCE = 1e-6;

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private ModelingOwnerRig rig;
    private long body;

    @Before
    public void startAtHome() {
        rig = new ModelingOwnerRig(rule.getScenario(), "modeling-surface", "MFSURF_DEVICE");
        rig.startAtHome();
    }

    @After
    public void restoreAProject() {
        rig.restore();
    }

    // =======================================================================
    // DEV-SURF-01: Patch, Trim, an extruded tube, Stitch; Thicken is not
    // offered for a stitched shell; Undo/Redo; save and reopen.
    // =======================================================================

    @Test
    public void devSurf01_patch_trim_tube_stitch_and_save_reopen() {
        newSurfaceProject();
        drawSquare(2.0);
        finishAs(R.id.surface_create_patch);
        assertTrue("the first Finish created the project", NativeViewport.projectOpen());
        body = NativeViewport.sceneActiveBodyId();
        assertEquals(NativeViewport.REPRESENTATION_SURFACE, NativeViewport.sceneBodyRepresentation(body));
        assertEquals("the new project starts with an empty history", 0, NativeViewport.constructionUndoDepth());
        assertEquals(1, state(NativeViewport.SURFACE_STATE_FEATURES), 0.0);
        assertEquals("one closed boundary edge", 1, state(NativeViewport.SURFACE_STATE_OPEN_EDGES), 0.0);
        assertEquals("a patch is drawn from both sides", 1.0, state(NativeViewport.SURFACE_STATE_TWO_SIDED), 0.0);
        rig.capture("01_patch");

        // Trim: a circle on the patch's own plane, cut out of it.
        newSketchOnBody("0");
        selectTool(rule.getScenario(), R.id.tool_rail_circle);
        dragSketch(rule.getScenario(), 0.0, 0.0, 0.5, 0.0);
        final long circle = selectedEntity();
        assertEquals(NativeViewport.CAD_OK, rig.applyOnUi(() -> NativeViewport.sketchApplyCircle(circle, 0.5)));
        rig.touchId(R.id.finish_sketch);
        assertTrue("Trim is offered over a coplanar patch", rig.shown(R.id.surface_create_trim));
        assertFalse("Loft is not offered without a first section", rig.shown(R.id.surface_create_loft));
        rig.touchId(R.id.surface_create_trim);
        assertEquals(NativeViewport.SKETCH_INACTIVE, sketchState());
        assertEquals(2, state(NativeViewport.SURFACE_STATE_FEATURES), 0.0);
        assertEquals("the hole is a second boundary edge", 2, state(NativeViewport.SURFACE_STATE_OPEN_EDGES), 0.0);
        rig.capture("02_trimmed");

        // A tube on the same square, 1 m, then Stitch.
        newSketchOnBody("0");
        drawSquare(2.0);
        rig.touchId(R.id.finish_sketch);
        setField(R.id.field_surface_distance, "1");
        rig.touchId(R.id.surface_create_extrude);
        assertEquals(3, state(NativeViewport.SURFACE_STATE_FEATURES), 0.0);
        final int openBefore = (int) state(NativeViewport.SURFACE_STATE_OPEN_EDGES);
        openSurfaceSurface();
        assertTrue("Stitch is drawn: its candidate succeeds", rig.shown(R.id.surface_stitch));
        rig.touchId(R.id.surface_stitch);
        assertEquals(4, state(NativeViewport.SURFACE_STATE_FEATURES), 0.0);
        assertEquals("the patch's outer loop and the tube's bottom are one seam", 1,
                state(NativeViewport.SURFACE_STATE_STITCHES), 0.0);
        assertEquals("two edges fewer are open", openBefore - 2, (int) state(NativeViewport.SURFACE_STATE_OPEN_EDGES));
        rig.capture("03_stitched");

        // A stitched shell has no exact offset: Thicken is not offered, and
        // native names why.
        openSurfaceSurface();
        assertEquals("no Thicken is drawn over a stitched shell", 0, thickenButtons().size());
        assertEquals("ThickenUnsupportedForSurfaceType",
                NativeViewport.surfaceStatusToken(NativeViewport.surfaceThicken(body, 3, 0.1, false)));

        // Undo removes exactly the Stitch; Redo restores it.
        final long stitched = NativeViewport.surfaceMeshDigest(body);
        rig.touchId(R.id.undo_action);
        assertEquals(3, state(NativeViewport.SURFACE_STATE_FEATURES), 0.0);
        assertEquals(0, state(NativeViewport.SURFACE_STATE_STITCHES), 0.0);
        rig.touchId(R.id.redo_action);
        assertEquals(stitched, NativeViewport.surfaceMeshDigest(body));

        saveAndReopen();
        rig.fact("surf01.digest", Long.toHexString(stitched));
    }

    // =======================================================================
    // DEV-SURF-02: an open curve extruded, thickened; the FIRST feature edited
    // through the History rebuilds the Thicken; a staged failure is named and
    // writes nothing.
    // =======================================================================

    @Test
    public void devSurf02_open_extrude_thicken_and_edit_the_first_feature() {
        newSurfaceProject();
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        placeLine(2.0, 0.0, 3.0, 0.0);
        placeLine(3.0, 0.0, 3.0, 1.0);
        rig.touchId(R.id.finish_sketch);
        assertFalse("an open chain fills nothing: no Patch", rig.shown(R.id.surface_create_patch));
        setField(R.id.field_surface_distance, "0.5");
        rig.touchId(R.id.surface_create_extrude);
        body = NativeViewport.sceneActiveBodyId();
        assertEquals(NativeViewport.REPRESENTATION_SURFACE, NativeViewport.sceneBodyRepresentation(body));
        assertEquals("an open wall: two chain sides, a top and a bottom", 4,
                state(NativeViewport.SURFACE_STATE_OPEN_EDGES), 0.0);
        assertEquals(0, state(NativeViewport.SURFACE_STATE_SOLID_TRIANGLES), 0.0);
        rig.capture("01_open_wall");

        // Thicken the wall 0.1 m.
        openSurfaceSurface();
        setField(R.id.field_surface_thickness, "0.1");
        final List<View> buttons = thickenButtons();
        assertEquals("one live feature can be thickened", 1, buttons.size());
        rig.touchView((activity, workspace) -> thickenButtons().get(0));
        final double thin = 0.19 * 0.5;  // the mitred L profile's area x the height
        assertEquals(thin, state(NativeViewport.SURFACE_STATE_SOLID_VOLUME), TOLERANCE);
        assertEquals("the solid replaced the patch", 0, state(NativeViewport.SURFACE_STATE_PATCHES), 0.0);
        rig.capture("02_thickened");

        // Edit the FIRST feature through the History: Extruded Surface 1, 0.8 m.
        openHistory();
        final int extrudeRow = rowIndex(false, 1);
        rig.touchView((activity, workspace) -> workspace.featureHistory().rowsContainer().getChildAt(extrudeRow));
        assertTrue("the staged edit opened", rig.on((a, w) -> w.surfaceEditor().editOpen()));
        assertEquals("the field holds the committed value", 0.5, fieldNumber(R.id.field_surface_edit_value), 1e-9);
        setField(R.id.field_surface_edit_value, "0.8");
        assertTrue("Apply is drawn: everything rebuilds", rig.shown(R.id.surface_edit_apply));
        assertEquals("nothing written before Apply", thin, state(NativeViewport.SURFACE_STATE_SOLID_VOLUME), TOLERANCE);
        rig.touchId(R.id.surface_edit_apply);
        assertEquals("the later Thicken rebuilt on the taller wall", 0.19 * 0.8,
                state(NativeViewport.SURFACE_STATE_SOLID_VOLUME), TOLERANCE);
        rig.capture("03_first_feature_edited");
        final int undoAfterEdit = NativeViewport.constructionUndoDepth();
        rig.touchId(R.id.undo_action);
        assertEquals("one Undo restores the edit", thin, state(NativeViewport.SURFACE_STATE_SOLID_VOLUME), TOLERANCE);
        rig.touchId(R.id.redo_action);
        assertEquals(undoAfterEdit, NativeViewport.constructionUndoDepth());

        // A staged failure: thickness 0 is refused at the Thicken itself, named,
        // Apply absent, Fix drawn; Cancel writes nothing.
        final byte[] before = NativeViewport.encodeProject();
        openHistory();
        final int thickenRow = rowIndex(false, 2);
        rig.touchView((activity, workspace) -> workspace.featureHistory().rowsContainer().getChildAt(thickenRow));
        setField(R.id.field_surface_edit_value, "0");
        assertFalse("Apply is withdrawn", rig.shown(R.id.surface_edit_apply));
        assertTrue("Fix is offered", rig.shown(R.id.surface_edit_fix));
        final String verdict = rig.on((a, w) ->
                ((TextView) w.surfaceEditor().findViewById(R.id.surface_edit_verdict)).getText().toString());
        rig.fact("surf02.staged_failure", verdict);
        assertTrue(verdict, verdict.contains(rig.on((a, w) -> a.getString(R.string.surface_kind_thicken))));
        rig.capture("04_staged_failure");
        rig.touchId(R.id.surface_edit_cancel);
        assertArrayEquals("Cancel wrote nothing", before, NativeViewport.encodeProject());
    }

    // =======================================================================
    // DEV-SURF-03: an open profile revolved about a Construction line; a first
    // section kept, then a Loft at a typed offset; save and reopen.
    // =======================================================================

    @Test
    public void devSurf03_revolve_an_open_profile_and_loft_two_sections() {
        newSurfaceProject();
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        placeLine(1.0, 0.0, 1.0, 1.0);
        rig.touchId(R.id.finish_sketch);
        assertFalse("no axis yet: Revolve is not offered", rig.shown(R.id.surface_create_revolve));
        // Back to the sketch: draw the axis and mark it Construction.
        rig.on((a, w) -> {
            WorkspaceTestSupport.closePrecision(w);
            return null;
        });
        settleLayout();
        selectTool(rule.getScenario(), R.id.tool_rail_line);
        placeLine(0.0, -1.0, 0.0, 2.0);
        openPalette();
        rig.touchId(R.id.sketch_action_construction);
        rig.touchId(R.id.finish_sketch);
        assertTrue("one Construction line is the axis: Revolve is offered", rig.shown(R.id.surface_create_revolve));
        setField(R.id.field_surface_angle, "360");
        rig.touchId(R.id.surface_create_revolve);
        body = NativeViewport.sceneActiveBodyId();
        assertEquals(1, state(NativeViewport.SURFACE_STATE_FEATURES), 0.0);
        assertEquals("a full turn of an open segment: two circular edges", 2,
                state(NativeViewport.SURFACE_STATE_OPEN_EDGES), 0.0);
        rig.capture("01_revolved");

        // The first section: a circle kept, not yet a feature.
        newSketchOnBody("0");
        drawCircle(3.0, 0.0, 0.5);
        rig.touchId(R.id.finish_sketch);
        assertFalse(rig.shown(R.id.surface_create_loft));
        rig.touchId(R.id.surface_create_section);
        assertEquals(1, state(NativeViewport.SURFACE_STATE_FEATURES), 0.0);
        assertNotEquals("a pending section", 0.0, state(NativeViewport.SURFACE_STATE_PENDING_SECTION));

        // The second, 1 m above it, drawn where it stands.
        newSketchOnBody("1");
        final double[] sketch = new double[NativeViewport.SURFACE_SKETCH_STATE_SIZE];
        NativeViewport.surfaceSketchState(0.5, 360, false, sketch);
        assertEquals("the sketch stands at its offset", 1.0, sketch[NativeViewport.SURFACE_SKETCH_OFFSET], 0.0);
        drawCircle(3.0, 0.0, 0.3);
        rig.touchId(R.id.finish_sketch);
        assertTrue("Loft is offered from the kept section", rig.shown(R.id.surface_create_loft));
        rig.touchId(R.id.surface_create_loft);
        assertEquals(2, state(NativeViewport.SURFACE_STATE_FEATURES), 0.0);
        assertEquals("no section is pending any more", 0, state(NativeViewport.SURFACE_STATE_PENDING_SECTION), 0.0);
        assertEquals("the loft adds two open circles", 4, state(NativeViewport.SURFACE_STATE_OPEN_EDGES), 0.0);
        rig.capture("02_lofted");
        saveAndReopen();
    }

    // -----------------------------------------------------------------------
    // Journey steps
    // -----------------------------------------------------------------------

    private void newSurfaceProject() {
        rig.touchId(R.id.home_new_project);
        rig.touchId(R.id.new_project_surface);
        assertEquals("New Surface lands in a sketch", NativeViewport.SKETCH_EDITING, sketchState());
        assertFalse("no project before the first Finish", NativeViewport.projectOpen());
    }

    /** The Surface surface over the active body, with Shape held. */
    private void openSurfaceSurface() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openPrecision(workspace);
            return null;
        });
        settleLayout();
        assertTrue("the Surface surface is open", rig.on((a, w) -> w.surfaceEditor().isShown()));
    }

    private void newSketchOnBody(String offset) {
        openSurfaceSurface();
        setField(R.id.field_surface_offset, offset);
        rig.touchId(R.id.surface_new_sketch);
        assertEquals("a Surface sketch is open", NativeViewport.SKETCH_EDITING, sketchState());
    }

    private void finishAs(int createId) {
        rig.touchId(R.id.finish_sketch);
        assertTrue("the Surface surface offers it", rig.shown(createId));
        rig.touchId(createId);
        assertEquals("the sketch is over: " + rig.statusLine(), NativeViewport.SKETCH_INACTIVE, sketchState());
    }

    /** A centred square by a real drag, its size typed exact. */
    private void drawSquare(double side) {
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), -side / 2, -side / 2, side / 2, side / 2);
        final long id = selectedEntity();
        assertEquals(NativeViewport.CAD_OK, rig.applyOnUi(() -> NativeViewport.sketchApplyRectangle(id, side, side)));
        final double[] drawn = entity();
        assertEquals("centred", 0.0, drawn[NativeViewport.SKETCH_ENTITY_VALUES], 1e-9);
        assertEquals("centred", 0.0, drawn[NativeViewport.SKETCH_ENTITY_VALUES + 1], 1e-9);
    }

    private void drawCircle(double u, double v, double radius) {
        selectTool(rule.getScenario(), R.id.tool_rail_circle);
        dragSketch(rule.getScenario(), u, v, u + radius, v);
        final long id = selectedEntity();
        assertEquals(NativeViewport.CAD_OK, rig.applyOnUi(() -> NativeViewport.sketchApplyCircle(id, radius)));
    }

    /** A line by a real drag, its ends typed exact. */
    private long placeLine(double u0, double v0, double u1, double v1) {
        final int before = sketchEntityCount();
        dragSketch(rule.getScenario(), u0, v0, u1, v1);
        assertEquals("one line placed", before + 1, sketchEntityCount());
        final long id = selectedEntity();
        assertEquals(NativeViewport.CAD_OK,
                rig.applyOnUi(() -> NativeViewport.sketchApplyLine(id, u0, v0, u1, v1)));
        return id;
    }

    private void openPalette() {
        final boolean open = rig.on((a, w) -> w.sketchModify().paletteOpen());
        if (!open) {
            rig.touchId(R.id.sketch_modify_toggle);
        }
        assertTrue("the palette is open", rig.on((a, w) -> w.sketchModify().paletteOpen()));
    }

    private void saveAndReopen() {
        final byte[] saved = NativeViewport.encodeProject();
        final long digest = NativeViewport.surfaceMeshDigest(body);
        final long fingerprint = NativeViewport.projectFingerprint();
        assertEquals(NativeViewport.PROJECT_OK, rig.applyOnUi(() -> NativeViewport.loadProject(saved)));
        rig.refresh();
        assertEquals("the same body", body, NativeViewport.sceneActiveBodyId());
        assertEquals("the same derived geometry", digest, NativeViewport.surfaceMeshDigest(body));
        assertEquals(fingerprint, NativeViewport.projectFingerprint());
        assertArrayEquals("reopening wrote nothing new", saved, NativeViewport.encodeProject());
        rig.fact("save.bytes", saved.length);
    }

    // -----------------------------------------------------------------------
    // History
    // -----------------------------------------------------------------------

    private void openHistory() {
        rig.on((a, w) -> {
            WorkspaceTestSupport.closePrecision(w);
            return null;
        });
        settleLayout();
        rig.refresh();
        assertTrue("the History control is drawn for a Surface body", rig.shown(R.id.feature_history_action));
        if (!rig.on((a, w) -> w.featureHistory().isOpen())) {
            rig.touchId(R.id.feature_history_action);
        }
        assertTrue(rig.on((a, w) -> w.featureHistory().isOpen()));
        final List<String> texts = rig.on((a, w) -> {
            final List<String> out = new ArrayList<>();
            final LinearLayout rows = w.featureHistory().rowsContainer();
            for (int i = 0; i < rows.getChildCount(); i++) {
                out.add(((TextView) rows.getChildAt(i)).getText().toString());
            }
            return out;
        });
        rig.fact("history.rows", texts);
    }

    /** The index of the row naming a sketch or feature by its durable id. */
    private int rowIndex(boolean sketch, long id) {
        final double[] header = new double[NativeViewport.TIMELINE_HEADER_SIZE];
        final double[] rows = new double[NativeViewport.TIMELINE_MAX_ROWS * 2 * NativeViewport.TIMELINE_ROW_SIZE];
        final int count = NativeViewport.surfaceTimeline(body, -1, 0L, 0.0, header, rows);
        final FeatureHistoryPresentation.Model model = FeatureHistoryPresentation.fromNative(
                FeatureHistoryPresentation.DOMAIN_SURFACE, header, rows, count);
        for (int i = 0; i < model.rows.size(); i++) {
            final FeatureHistoryPresentation.Row row = model.rows.get(i);
            if (row.isSketch() == sketch && row.id == id) {
                return i;
            }
        }
        throw new AssertionError("no row " + (sketch ? "S" : "F") + id);
    }

    // -----------------------------------------------------------------------
    // Plumbing
    // -----------------------------------------------------------------------

    private List<View> thickenButtons() {
        return rig.on((activity, workspace) -> {
            final List<View> out = new ArrayList<>();
            ModelingOwnerRig.collectById(workspace.surfaceEditor(), R.id.surface_thicken, out);
            return out;
        });
    }

    private void setField(final int fieldId, final String text) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final EditText field = workspace.surfaceEditor().rowFor(fieldId).field();
            field.setText(text);
            return null;
        });
        settleLayout();
    }

    private double fieldNumber(final int fieldId) {
        return rig.on((a, w) -> Double.parseDouble(w.surfaceEditor().rowFor(fieldId).text()));
    }

    private double state(int slot) {
        final double[] out = new double[NativeViewport.SURFACE_STATE_SIZE];
        assertTrue("the body is a Surface body", NativeViewport.surfaceState(body, out));
        return out[slot];
    }

    private static long selectedEntity() {
        return (long) entity()[NativeViewport.SKETCH_ENTITY_ID];
    }

    private static double[] entity() {
        final double[] drawn = new double[NativeViewport.SKETCH_ENTITY_SIZE];
        assertTrue("an entity is selected", NativeViewport.sketchSelectedEntity(drawn));
        return drawn;
    }
}
