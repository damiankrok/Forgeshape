package com.forgeshape.app;

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

import android.net.Uri;
import android.os.SystemClock;
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

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

/**
 * `E2E-CADR0-01..16`: a sketch on a workplane becomes an editable CAD Body,
 * through the real chrome and real MotionEvents.
 *
 * <h2>What this suite is for</h2>
 *
 * <p>The DOMAIN half — workplane mapping, entity rules, profile extraction,
 * triangulation, the watertight extrusion, the history step, the `.forge`
 * branch and the export — is proved by the native `CADR0-*` self-test, which
 * builds its own scenes and its own camera. What is left, and what this covers,
 * is everything that can only be true on a device: that the palette's New
 * Sketch opens a plane chooser, that the toolbar carries Finish Sketch and then
 * Extrude, that a drag across the {@code SurfaceView} places a rectangle through
 * the whole production touch path, that the precision surface's typed depth is
 * what the body is extruded to, that the new body has an Objects row and a
 * gizmo, that its sizes and depth are editable later as one step each, and that
 * the sculpt workflow beside it is untouched.
 *
 * <h2>Rules</h2>
 *
 * <p>No control is located by coordinate. The one place a pixel appears is the
 * viewport gesture, and every such pixel is asked for from
 * {@code sketchScreenPoint} or {@code gizmoHandlePoint} — the same projection
 * native code unprojects with — never written down.
 */
@RunWith(AndroidJUnit4.class)
public final class SketchExtrudeTest {

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
    // E2E-CADR0-01/02: New Sketch, and a plane
    // -----------------------------------------------------------------------

    @Test
    public void e2eCadr0_01and02_newSketchAsksForAPlaneAndBeginsOnIt() {
        final int bodiesBefore = NativeViewport.sceneBodyCount();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openAddPrimitive(workspace);
            assertFalse("the palette opens on the shapes",
                    workspace.addPrimitivePalette().showingPlanes());
            final View newSketch = workspace.addPrimitivePalette().findViewById(R.id.add_sketch);
            assertNotNull("E2E-CADR0-01: the palette offers New Sketch", newSketch);
            // Since CAD-A3 the tile lands directly in the spatial support
            // chooser (UI-OWNER-46); the three named planes stay reachable
            // from the secondary control beside it, which is the path this
            // by-name case takes.
            newSketch.performClick();
            assertTrue("E2E-CADR0-01: New Sketch is a viewport-first support pick",
                    NativeViewport.supportChooserActive());
            assertEquals("no sketch has begun yet", NativeViewport.SKETCH_INACTIVE, sketchState());
            NativeViewport.supportChooserCancel();
            WorkspaceTestSupport.openAddPrimitive(workspace);
            workspace.addPrimitivePalette().findViewById(R.id.sketch_plane_by_name).performClick();
            assertTrue("E2E-CADR0-01: the by-name list asks which plane",
                    workspace.addPrimitivePalette().showingPlanes());
            for (int id : new int[]{R.id.sketch_plane_xy, R.id.sketch_plane_xz,
                    R.id.sketch_plane_yz}) {
                assertNotNull("all three principal planes are offered",
                        workspace.addPrimitivePalette().findViewById(id));
            }
            workspace.addPrimitivePalette().findViewById(R.id.sketch_plane_xz).performClick();
            return null;
        });
        settleLayout();
        assertEquals("E2E-CADR0-02: a sketch is open", NativeViewport.SKETCH_EDITING, sketchState());
        assertEquals("on the plane that was chosen", NativeViewport.WORKPLANE_XZ, sketchPlane());
        assertEquals("beginning a sketch creates nothing", bodiesBefore, NativeViewport.sceneBodyCount());
        assertEquals("and records nothing", 0, NativeViewport.constructionUndoDepth());
        assertEquals("the palette closed", false, paletteOpen());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("the toolbar names the plane",
                    activity.getString(R.string.context_sketch,
                            activity.getString(R.string.workplane_xz)),
                    ((TextView) workspace.findViewById(R.id.editing_context_label)).getText()
                            .toString());
            assertEquals("Finish Sketch is the one transition", View.VISIBLE,
                    workspace.findViewById(R.id.finish_sketch).getVisibility());
            assertEquals("Extrude is not yet offered", View.GONE,
                    workspace.findViewById(R.id.extrude_sketch).getVisibility());
            assertEquals("Start Sculpting is withdrawn", View.GONE,
                    workspace.findViewById(R.id.freeze_to_sculpt).getVisibility());
            assertEquals("the sketch's Cancel is under the rail", View.VISIBLE,
                    workspace.findViewById(R.id.cancel_sketch).getVisibility());
            assertEquals("Back to Sketch is not, yet", View.GONE,
                    workspace.findViewById(R.id.back_to_sketch).getVisibility());
            assertEquals("Undo and Redo are withdrawn while sketching", View.GONE,
                    workspace.historyGroup().getVisibility());
            assertFalse("creation is withdrawn while sketching",
                    workspace.objectsCapsule().creationAvailable());
            for (int id : new int[]{R.id.tool_rail_select, R.id.tool_rail_line,
                    R.id.tool_rail_polyline, R.id.tool_rail_rectangle, R.id.tool_rail_circle}) {
                final View entry = workspace.findViewById(id);
                assertNotNull("the rail carries the sketch tools", entry);
                final float density = activity.getResources().getDisplayMetrics().density;
                assertTrue("and each meets the 48 dp floor",
                        entry.getHeight() >= Math.round(48f * density) - 1);
            }
            assertEquals("the rail draws the held tool, read back from native",
                    Integer.valueOf(NativeViewport.sketchTool()),
                    WorkspaceTestSupport.toolRail(workspace).activeKey());
            final double[] gizmo = new double[NativeViewport.GIZMO_STATE_SIZE];
            NativeViewport.gizmoState(gizmo);
            assertEquals("there is no gizmo over a sketch", 0.0,
                    gizmo[NativeViewport.GIZMO_ACTIVE], 0.0);
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // E2E-CADR0-03..08: a rectangle, finished, extruded, undone, redone
    // -----------------------------------------------------------------------

    @Test
    public void e2eCadr0_03to08_aDraggedRectangleBecomesOneUndoableCadBody() {
        beginSketch(NativeViewport.WORKPLANE_XY);
        final int bodiesBefore = NativeViewport.sceneBodyCount();
        final int undoBefore = NativeViewport.constructionUndoDepth();

        // E2E-CADR0-03: a real drag across the viewport, with the Rectangle
        // tool the sketch opens on.
        selectSketchTool(R.id.tool_rail_rectangle);
        dragSketch(-1.0, -0.5, 1.0, 0.5);
        assertEquals("E2E-CADR0-03: the drag placed one entity", 1, sketchEntityCount());
        final double[] entity = new double[NativeViewport.SKETCH_ENTITY_SIZE];
        assertTrue("and selected it", NativeViewport.sketchSelectedEntity(entity));
        assertEquals("it is a rectangle", NativeViewport.SKETCH_ENTITY_KIND_RECTANGLE,
                (int) entity[NativeViewport.SKETCH_ENTITY_KIND]);
        // The grid is view-adaptive since CAD-A3, so the snapped width and
        // height land on exact multiples of the CURRENT step (whatever that is
        // at this zoom), which is the real invariant. The dragged span is
        // positive and grid-aligned.
        final double gridStep = NativeViewport.sketchGridStep();
        final double width = entity[NativeViewport.SKETCH_ENTITY_VALUES + 2];
        final double height = entity[NativeViewport.SKETCH_ENTITY_VALUES + 3];
        assertTrue("the width is positive and on the grid",
                width > 0.0 && onGrid(width, gridStep));
        assertTrue("the height is positive and on the grid",
                height > 0.0 && onGrid(height, gridStep));

        // E2E-CADR0-04: Finish Sketch through the toolbar.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.finish_sketch).performClick();
            return null;
        });
        settleLayout();
        assertEquals("E2E-CADR0-04: the sketch is ready", NativeViewport.SKETCH_READY, sketchState());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("Extrude is now the one transition", View.VISIBLE,
                    workspace.findViewById(R.id.extrude_sketch).getVisibility());
            assertEquals("and Finish Sketch has gone", View.GONE,
                    workspace.findViewById(R.id.finish_sketch).getVisibility());
            assertEquals("Back to Sketch is offered", View.VISIBLE,
                    workspace.findViewById(R.id.back_to_sketch).getVisibility());
            assertTrue("the precision surface opened on the profile and the depth",
                    workspace.propertyInspector().isOpen());
            assertNotNull("with the one profile listed",
                    workspace.sketchEditor().findViewById(R.id.sketch_profile_option));
            assertNotNull("and a depth field",
                    workspace.sketchEditor().findViewById(R.id.field_extrude_depth));
            return null;
        });

        // E2E-CADR0-05: a typed depth, then Extrude from the pinned commit.
        final long created = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final EditText depth =
                    workspace.sketchEditor().findViewById(R.id.field_extrude_depth);
            depth.setText("2.5");
            workspace.findViewById(R.id.sketch_extrude_commit).performClick();
            return NativeViewport.sceneActiveBodyId();
        });
        settleLayout();
        assertEquals("E2E-CADR0-05: one body was created", bodiesBefore + 1,
                NativeViewport.sceneBodyCount());
        assertEquals("the session is over", NativeViewport.SKETCH_INACTIVE, sketchState());
        assertEquals("E2E-CADR0-08: creation is exactly one history step", undoBefore + 1,
                NativeViewport.constructionUndoDepth());
        assertTrue("the new body is a CAD Body", NativeViewport.sceneActiveBodyIsCad());
        final double[] cad = new double[NativeViewport.CAD_STATE_SIZE];
        assertTrue(NativeViewport.cadState(cad));
        assertEquals("extruded to the typed depth", 2.5, cad[NativeViewport.CAD_DEPTH], 0.0);
        assertEquals("from the rectangle profile", NativeViewport.CAD_PROFILE_RECTANGLE,
                (int) cad[NativeViewport.CAD_PROFILE_KIND]);
        // The sizes are the dragged span snapped to the adaptive grid: positive
        // multiples of the current step, not a hardcoded 2.0 x 1.0.
        final double primary = cad[NativeViewport.CAD_PRIMARY_SIZE];
        final double secondary = cad[NativeViewport.CAD_SECONDARY_SIZE];
        assertTrue("the width is positive and on the grid",
                primary > 0.0 && onGrid(primary, NativeViewport.sketchGridStep()));
        assertTrue("the height is positive and on the grid",
                secondary > 0.0 && onGrid(secondary, NativeViewport.sketchGridStep()));
        assertNotEquals("and it has a published mesh", 0L,
                NativeViewport.constructionMeshRevision());

        // E2E-CADR0-06: the body appears in Objects, and the chrome says CAD.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openObjectsPanel(workspace);
            assertNotNull("E2E-CADR0-06: the Objects list has a row for it",
                    workspace.objectsSection().rowFor(created));
            WorkspaceTestSupport.closeObjectsPanel(workspace);
            assertEquals("the toolbar names the context",
                    activity.getString(R.string.context_cad_body),
                    ((TextView) workspace.findViewById(R.id.editing_context_label)).getText()
                            .toString());
            assertEquals("Start Sculpting is not offered for a CAD Body", View.GONE,
                    workspace.findViewById(R.id.freeze_to_sculpt).getVisibility());
            assertEquals("Undo and Redo are back", View.VISIBLE,
                    workspace.historyGroup().getVisibility());
            assertTrue("creation is back", workspace.objectsCapsule().creationAvailable());
            return null;
        });

        // E2E-CADR0-07/08: Undo removes the whole body; Redo brings the same one back.
        final byte[] withBody = NativeViewport.encodeProject();
        clickUndo();
        assertEquals("E2E-CADR0-07: Undo removed the body", bodiesBefore,
                NativeViewport.sceneBodyCount());
        assertNull("no CAD state to read", cadStateOrNull());
        clickRedo();
        assertEquals("E2E-CADR0-08: Redo restored it", bodiesBefore + 1,
                NativeViewport.sceneBodyCount());
        assertArrayEquals("as the same document", withBody, NativeViewport.encodeProject());
        assertNotNull(NativeViewport.sceneBodyRepresentation(created) == NativeViewport.REPRESENTATION_CAD
                ? Boolean.TRUE : null);
    }

    // -----------------------------------------------------------------------
    // E2E-CADR0-09/10: editing the CAD Body later
    // -----------------------------------------------------------------------

    @Test
    public void e2eCadr0_09and10_theRectangleAndTheDepthAreEditableLaterAsOneStepEach() {
        final long created = extrudeARectangle(2.0, 1.0, 1.5);
        final int undoBefore = NativeViewport.constructionUndoDepth();
        final long revisionBefore = NativeViewport.constructionMeshRevision();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.selectConstructionTool(workspace, R.id.tool_rail_shape);
            WorkspaceTestSupport.openPrecision(workspace);
            assertTrue("Shape on a CAD Body is its sketch and extrusion",
                    workspace.cadEditor().isAttachedToWindow());
            final EditText width = workspace.cadEditor().findViewById(R.id.field_cad_rect_width);
            final EditText height =
                    workspace.cadEditor().findViewById(R.id.field_cad_rect_height);
            assertEquals("the fields show the authored sizes", "2", width.getText().toString().trim());
            assertEquals("1", height.getText().toString().trim());
            width.setText("4");
            height.setText("3");
            workspace.findViewById(R.id.apply_cad).performClick();
            return null;
        });
        settleLayout();
        double[] cad = new double[NativeViewport.CAD_STATE_SIZE];
        assertTrue(NativeViewport.cadState(cad));
        assertEquals("E2E-CADR0-09: the width was applied", 4.0, cad[NativeViewport.CAD_PRIMARY_SIZE], 0.0);
        assertEquals("and the height", 3.0, cad[NativeViewport.CAD_SECONDARY_SIZE], 0.0);
        assertEquals("as one history step", undoBefore + 1, NativeViewport.constructionUndoDepth());
        assertNotEquals("and the mesh was regenerated", revisionBefore,
                NativeViewport.constructionMeshRevision());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final EditText depth = workspace.cadEditor().findViewById(R.id.field_cad_depth);
            depth.setText("0.75");
            workspace.findViewById(R.id.apply_cad).performClick();
            return null;
        });
        settleLayout();
        assertTrue(NativeViewport.cadState(cad));
        assertEquals("E2E-CADR0-10: the depth was applied", 0.75, cad[NativeViewport.CAD_DEPTH], 0.0);
        assertEquals("as one more step", undoBefore + 2, NativeViewport.constructionUndoDepth());

        clickUndo();
        assertTrue(NativeViewport.cadState(cad));
        assertEquals("Undo restores the previous depth", 1.5, cad[NativeViewport.CAD_DEPTH], 0.0);
        assertEquals("and leaves the sizes", 4.0, cad[NativeViewport.CAD_PRIMARY_SIZE], 0.0);
        clickUndo();
        assertTrue(NativeViewport.cadState(cad));
        assertEquals("a second Undo restores the sizes", 2.0, cad[NativeViewport.CAD_PRIMARY_SIZE], 0.0);
        clickRedo();
        clickRedo();
        assertTrue(NativeViewport.cadState(cad));
        assertEquals("Redo reapplies both", 0.75, cad[NativeViewport.CAD_DEPTH], 0.0);
        assertEquals(3.0, cad[NativeViewport.CAD_SECONDARY_SIZE], 0.0);

        // A refused edit changes nothing and records nothing.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final EditText width = workspace.cadEditor().findViewById(R.id.field_cad_rect_width);
            width.setText("0");
            workspace.findViewById(R.id.apply_cad).performClick();
            return null;
        });
        settleLayout();
        assertTrue(NativeViewport.cadState(cad));
        assertEquals("a zero width is refused", 4.0, cad[NativeViewport.CAD_PRIMARY_SIZE], 0.0);
        assertEquals("and records no step", undoBefore + 2, NativeViewport.constructionUndoDepth());
        assertEquals(created, NativeViewport.sceneActiveBodyId());
    }

    // -----------------------------------------------------------------------
    // E2E-CADR0-11: Save, reopen, still editable
    // -----------------------------------------------------------------------

    @Test
    public void e2eCadr0_11_aSavedCadBodyReopensAsEditableTruth() {
        final long created = extrudeARectangle(2.0, 1.0, 1.5);
        final long fingerprintBefore = NativeViewport.projectFingerprint();
        final byte[] saved = NativeViewport.encodeProject();
        assertEquals("the document validates", NativeViewport.PROJECT_OK,
                NativeViewport.validateProject(saved));

        // Something different in between, so the load is a real change.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.APPLY_APPLIED,
                    NativeViewport.cadApplyExtrude(3.0, NativeViewport.EXTRUDE_AGAINST_NORMAL));
            workspace.onNativeStateChanged();
            return null;
        });
        assertNotEquals("the fingerprint follows a CAD edit", fingerprintBefore,
                NativeViewport.projectFingerprint());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.PROJECT_OK, NativeViewport.loadProject(saved));
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        assertEquals("E2E-CADR0-11: the reopened project is the saved one",
                fingerprintBefore, NativeViewport.projectFingerprint());
        assertArrayEquals(saved, NativeViewport.encodeProject());
        assertEquals("the CAD Body came back as one", NativeViewport.REPRESENTATION_CAD,
                NativeViewport.sceneBodyRepresentation(created));
        final double[] cad = new double[NativeViewport.CAD_STATE_SIZE];
        assertTrue(NativeViewport.cadState(cad));
        assertEquals(1.5, cad[NativeViewport.CAD_DEPTH], 0.0);
        assertEquals("and is still editable", NativeViewport.APPLY_APPLIED,
                NativeViewport.cadApplyRectangle(5.0, 2.0, 1.5, NativeViewport.EXTRUDE_ALONG_NORMAL));
        assertEquals("a load starts a fresh history, and the edit is its first step", 1,
                NativeViewport.constructionUndoDepth());
    }

    // -----------------------------------------------------------------------
    // E2E-CADR0-12/13: a circle, and a closed polyline
    // -----------------------------------------------------------------------

    @Test
    public void e2eCadr0_12_aDraggedCircleExtrudes() {
        beginSketch(NativeViewport.WORKPLANE_YZ);
        final int bodiesBefore = NativeViewport.sceneBodyCount();
        selectSketchTool(R.id.tool_rail_circle);
        dragSketch(0.0, 0.0, 0.75, 0.0);
        assertEquals(1, sketchEntityCount());
        final double[] entity = new double[NativeViewport.SKETCH_ENTITY_SIZE];
        assertTrue(NativeViewport.sketchSelectedEntity(entity));
        assertEquals(NativeViewport.SKETCH_ENTITY_KIND_CIRCLE,
                (int) entity[NativeViewport.SKETCH_ENTITY_KIND]);
        // The radius is the dragged distance snapped to the adaptive grid: a
        // positive multiple of the current step, not a hardcoded 0.75.
        final double radius = entity[NativeViewport.SKETCH_ENTITY_VALUES + 2];
        assertTrue("the radius is positive and on the grid",
                radius > 0.0 && onGrid(radius, NativeViewport.sketchGridStep()));

        finishAndExtrude("1.25");
        assertEquals("E2E-CADR0-12: the circle became a body", bodiesBefore + 1,
                NativeViewport.sceneBodyCount());
        final double[] cad = new double[NativeViewport.CAD_STATE_SIZE];
        assertTrue(NativeViewport.cadState(cad));
        assertEquals(NativeViewport.CAD_PROFILE_CIRCLE, (int) cad[NativeViewport.CAD_PROFILE_KIND]);
        assertEquals("the extruded radius is the sketched one", radius,
                cad[NativeViewport.CAD_PRIMARY_SIZE], 1e-9);
        assertEquals(1.25, cad[NativeViewport.CAD_DEPTH], 0.0);
        assertEquals(NativeViewport.WORKPLANE_YZ, (int) cad[NativeViewport.CAD_PLANE]);
        // And its radius is editable later.
        assertEquals(NativeViewport.APPLY_APPLIED,
                NativeViewport.cadApplyCircle(0.5, 1.25, NativeViewport.EXTRUDE_ALONG_NORMAL));
        assertTrue(NativeViewport.cadState(cad));
        assertEquals(0.5, cad[NativeViewport.CAD_PRIMARY_SIZE], 0.0);
    }

    @Test
    public void e2eCadr0_13_aTappedPolylineClosesOnItsFirstPointAndExtrudes() {
        beginSketch(NativeViewport.WORKPLANE_XY);
        final int bodiesBefore = NativeViewport.sceneBodyCount();
        selectSketchTool(R.id.tool_rail_polyline);
        tapSketch(0.0, 0.0);
        tapSketch(2.0, 0.0);
        tapSketch(2.0, 1.0);
        tapSketch(1.0, 2.0);
        assertEquals("the polyline is still being placed", 1, polylineInProgress());
        tapSketch(0.0, 0.0);
        assertEquals("tapping the first point closed it", 0, polylineInProgress());
        assertEquals(1, sketchEntityCount());
        final double[] entity = new double[NativeViewport.SKETCH_ENTITY_SIZE];
        assertTrue(NativeViewport.sketchSelectedEntity(entity));
        assertEquals(NativeViewport.SKETCH_ENTITY_KIND_POLYLINE,
                (int) entity[NativeViewport.SKETCH_ENTITY_KIND]);
        assertEquals("four points", 4.0, entity[NativeViewport.SKETCH_ENTITY_VALUES], 0.0);
        assertEquals("closed", 1.0, entity[NativeViewport.SKETCH_ENTITY_VALUES + 1], 0.0);

        finishAndExtrude("0.5");
        assertEquals("E2E-CADR0-13: the polygon became a body", bodiesBefore + 1,
                NativeViewport.sceneBodyCount());
        final double[] cad = new double[NativeViewport.CAD_STATE_SIZE];
        assertTrue(NativeViewport.cadState(cad));
        assertEquals(NativeViewport.CAD_PROFILE_POLYGON, (int) cad[NativeViewport.CAD_PROFILE_KIND]);
        assertEquals(4.0, cad[NativeViewport.CAD_PROFILE_VERTICES], 0.0);
    }

    // -----------------------------------------------------------------------
    // E2E-CADR0-14: an open polyline is refused, and Cancel keeps nothing
    // -----------------------------------------------------------------------

    @Test
    public void e2eCadr0_14_anOpenPolylineIsRefusedByNameAndCancelChangesNothing() {
        final byte[] before = NativeViewport.encodeProject();
        final long fingerprint = NativeViewport.projectFingerprint();
        beginSketch(NativeViewport.WORKPLANE_XY);
        selectSketchTool(R.id.tool_rail_polyline);
        tapSketch(0.0, 0.0);
        tapSketch(2.0, 0.0);
        tapSketch(2.0, 2.0);
        tapSketch(2.0, 2.0);  // tapping the last point again ends it OPEN
        assertEquals(0, polylineInProgress());
        assertEquals(1, sketchEntityCount());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.finish_sketch).performClick();
            return null;
        });
        settleLayout();
        assertEquals("E2E-CADR0-14: an open profile does not finish", NativeViewport.SKETCH_EDITING,
                sketchState());
        assertEquals("and the refusal is named", NativeViewport.CAD_OPEN_PROFILE,
                NativeViewport.sketchLastStatus());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("Extrude is not offered over a refused sketch", View.GONE,
                    workspace.findViewById(R.id.extrude_sketch).getVisibility());
            // Delete the entity through the precision surface and draw nothing:
            // an EMPTY sketch is refused too.
            WorkspaceTestSupport.openPrecision(workspace);
            workspace.findViewById(R.id.delete_sketch_entity).performClick();
            return null;
        });
        settleLayout();
        assertEquals("Delete removed the entity", 0, sketchEntityCount());
        assertEquals(NativeViewport.CAD_NO_CLOSED_PROFILE, NativeViewport.sketchFinish());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.cancel_sketch).performClick();
            return null;
        });
        settleLayout();
        assertEquals("Cancel ends the session", NativeViewport.SKETCH_INACTIVE, sketchState());
        assertArrayEquals("and the project is byte-for-byte what it was", before,
                NativeViewport.encodeProject());
        assertEquals(fingerprint, NativeViewport.projectFingerprint());
        assertEquals("and nothing was recorded", 0, NativeViewport.constructionUndoDepth());
    }

    // -----------------------------------------------------------------------
    // E2E-CADR0-15: the gizmo moves a CAD Body, and a depth edit keeps the move
    // -----------------------------------------------------------------------

    @Test
    public void e2eCadr0_15_aGizmoDragMovesTheCadBodyAndADepthEditKeepsThePlacement() {
        extrudeARectangle(2.0, 1.0, 1.0);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.selectConstructionTool(workspace, R.id.tool_rail_place);
            assertEquals(View.VISIBLE,
                    WorkspaceTestSupport.transformSelectorRow(workspace).getVisibility());
            return null;
        });
        settleLayout();
        final double[] gizmo = new double[NativeViewport.GIZMO_STATE_SIZE];
        NativeViewport.gizmoState(gizmo);
        assertEquals("E2E-CADR0-15: the CAD Body has a gizmo", 1.0,
                gizmo[NativeViewport.GIZMO_VISIBLE], 0.0);

        final double[] before = new double[NativeViewport.TRANSFORM_SIZE];
        NativeViewport.boxTransform(before);
        final float[] pivot = new float[2];
        final float[] handle = new float[2];
        assertTrue(NativeViewport.gizmoHandlePoint(NativeViewport.GIZMO_HANDLE_NONE, pivot));
        assertTrue(NativeViewport.gizmoHandlePoint(NativeViewport.GIZMO_HANDLE_AXIS_X, handle));
        final float dx = handle[0] - pivot[0];
        final float dy = handle[1] - pivot[1];
        final float length = (float) Math.sqrt(dx * dx + dy * dy);
        dragViewport(handle[0], handle[1], handle[0] + dx / length * 80f, handle[1] + dy / length * 80f);
        final double[] moved = new double[NativeViewport.TRANSFORM_SIZE];
        NativeViewport.boxTransform(moved);
        assertNotEquals("the drag moved the body along X", before[NativeViewport.TRANSFORM_POSITION],
                moved[NativeViewport.TRANSFORM_POSITION], 1e-6);
        final int undoAfterDrag = NativeViewport.constructionUndoDepth();

        assertEquals(NativeViewport.APPLY_APPLIED,
                NativeViewport.cadApplyExtrude(2.0, NativeViewport.EXTRUDE_ALONG_NORMAL));
        final double[] after = new double[NativeViewport.TRANSFORM_SIZE];
        NativeViewport.boxTransform(after);
        assertArrayEquals("a depth edit leaves the placement exactly where the drag put it",
                moved, after, 0.0);
        assertEquals("and is its own step", undoAfterDrag + 1, NativeViewport.constructionUndoDepth());
    }

    // -----------------------------------------------------------------------
    // E2E-CADR0-16: Imported Mesh Sculpt Undo/Redo beside a CAD Body
    // -----------------------------------------------------------------------

    @Test
    public void e2eCadr0_16_importedMeshSculptUndoRedoStillWorksBesideACadBody() {
        extrudeARectangle(2.0, 1.0, 1.0);
        final long[] before = sceneBodyIds();
        // The same external-GLB import the Delete and Imported Mesh Sculpt
        // suites already prove: a real file through the product's own document
        // path, then the first object it created, at the identity placement.
        final File file = new File(
                InstrumentationRegistry.getInstrumentation().getTargetContext().getCacheDir(),
                "sketch-extrude-external.glb");
        writeFile(file, readAsset("glb/construction_sentinel.glb"));
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onOpenGlbDocumentChosen(Uri.fromFile(file));
            return null;
        });
        settleLayout();
        final long[] after = sceneBodyIds();
        assertTrue("the import added bodies", after.length > before.length);
        final long imported = after[before.length];
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(imported));
            assertTrue(NativeViewport.sceneActiveBodyIsImported());
            assertEquals(NativeViewport.APPLY_APPLIED,
                    NativeViewport.applyBoxTransform(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0));
            NativeViewport.setSculptBrush(300.0, 1.0);
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.freezeToSculpt());
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        assertEquals("E2E-CADR0-16: the stroke is one sculpt step", 1,
                NativeViewport.sculptUndoDepth());
        clickUndo();
        assertEquals("Undo took it back", 0, NativeViewport.sculptUndoDepth());
        assertEquals(1, NativeViewport.sculptRedoDepth());
        clickRedo();
        assertEquals("Redo put it back", 1, NativeViewport.sculptUndoDepth());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.enterConstructionMode();
            workspace.onNativeStateChanged();
            return null;
        });
        // The CAD Body is untouched by any of it.
        final long[] ids = sceneBodyIds();
        boolean cadStillThere = false;
        for (long id : ids) {
            cadStillThere |= NativeViewport.sceneBodyRepresentation(id)
                    == NativeViewport.REPRESENTATION_CAD;
        }
        assertTrue(cadStillThere);
    }

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    private void beginSketch(int plane) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openAddPrimitive(workspace);
            // The by-name fallback: the tile itself enters the spatial chooser.
            workspace.addPrimitivePalette().findViewById(R.id.sketch_plane_by_name).performClick();
            final int id = plane == NativeViewport.WORKPLANE_XZ ? R.id.sketch_plane_xz
                    : plane == NativeViewport.WORKPLANE_YZ ? R.id.sketch_plane_yz
                            : R.id.sketch_plane_xy;
            workspace.addPrimitivePalette().findViewById(id).performClick();
            return null;
        });
        settleLayout();
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
        assertEquals(plane, sketchPlane());
    }

    private void selectSketchTool(int entryId) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(entryId).performClick();
            return null;
        });
        settleLayout();
    }

    private void finishAndExtrude(String depth) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.finish_sketch).performClick();
            return null;
        });
        settleLayout();
        assertEquals(NativeViewport.SKETCH_READY, sketchState());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final EditText field =
                    workspace.sketchEditor().findViewById(R.id.field_extrude_depth);
            field.setText(depth);
            workspace.findViewById(R.id.sketch_extrude_commit).performClick();
            return null;
        });
        settleLayout();
        assertEquals(NativeViewport.SKETCH_INACTIVE, sketchState());
    }

    /**
     * A rectangle sketch on XY, extruded, through the real chrome. The drag is
     * from the origin to (width, height) rather than centred, so that integer
     * dimensions land on the adaptive grid exactly and the authored sizes are
     * predictable regardless of the current step.
     */
    private long extrudeARectangle(double width, double height, double depth) {
        beginSketch(NativeViewport.WORKPLANE_XY);
        selectSketchTool(R.id.tool_rail_rectangle);
        dragSketch(0.0, 0.0, width, height);
        assertEquals(1, sketchEntityCount());
        finishAndExtrude(Double.toString(depth));
        assertTrue(NativeViewport.sceneActiveBodyIsCad());
        return NativeViewport.sceneActiveBodyId();
    }

    private int sketchState() {
        final double[] state = new double[NativeViewport.SKETCH_STATE_SIZE];
        NativeViewport.sketchState(state);
        return (int) state[NativeViewport.SKETCH_STATE];
    }

    private int sketchPlane() {
        final double[] state = new double[NativeViewport.SKETCH_STATE_SIZE];
        NativeViewport.sketchState(state);
        return (int) state[NativeViewport.SKETCH_PLANE];
    }

    private int sketchEntityCount() {
        final double[] state = new double[NativeViewport.SKETCH_STATE_SIZE];
        NativeViewport.sketchState(state);
        return (int) state[NativeViewport.SKETCH_ENTITY_COUNT];
    }

    private int polylineInProgress() {
        final double[] state = new double[NativeViewport.SKETCH_STATE_SIZE];
        NativeViewport.sketchState(state);
        return (int) state[NativeViewport.SKETCH_POLYLINE_IN_PROGRESS];
    }

    private double[] cadStateOrNull() {
        final double[] cad = new double[NativeViewport.CAD_STATE_SIZE];
        return NativeViewport.cadState(cad) ? cad : null;
    }

    private boolean paletteOpen() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.addPrimitivePalette().isOpen());
    }

    private long[] sceneBodyIds() {
        final long[] ids = new long[Math.max(NativeViewport.sceneBodyCount(), 1)];
        final int written = NativeViewport.sceneBodyIds(ids);
        final long[] out = new long[written];
        System.arraycopy(ids, 0, out, 0, written);
        return out;
    }

    private void clickUndo() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.undoAction().performClick();
            return null;
        });
        settleLayout();
    }

    private void clickRedo() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.redoAction().performClick();
            return null;
        });
        settleLayout();
    }

    /**
     * A drag across the viewport between two SKETCH points, through the real
     * {@code SurfaceView}. The pixels are asked for from the same projection
     * native code unprojects with, never written down.
     */
    private void dragSketch(double u0, double v0, double u1, double v1) {
        final float[] from = new float[2];
        final float[] to = new float[2];
        assertTrue(NativeViewport.sketchScreenPoint(u0, v0, from));
        assertTrue(NativeViewport.sketchScreenPoint(u1, v1, to));
        dragViewport(from[0], from[1], to[0], to[1]);
    }

    private void tapSketch(double u, double v) {
        final float[] at = new float[2];
        assertTrue(NativeViewport.sketchScreenPoint(u, v, at));
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final long down = SystemClock.uptimeMillis();
            send(viewport, down, down, MotionEvent.ACTION_DOWN, at[0], at[1]);
            send(viewport, down, down + 40L, MotionEvent.ACTION_UP, at[0], at[1]);
            return null;
        });
        settleLayout();
    }

    private void dragViewport(float x0, float y0, float x1, float y1) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            assertTrue("precondition: the viewport is laid out",
                    viewport.getWidth() > 0 && viewport.getHeight() > 0);
            final long down = SystemClock.uptimeMillis();
            send(viewport, down, down, MotionEvent.ACTION_DOWN, x0, y0);
            for (int step = 1; step <= 6; ++step) {
                final float t = step / 6f;
                send(viewport, down, down + step * 12L, MotionEvent.ACTION_MOVE,
                        x0 + (x1 - x0) * t, y0 + (y1 - y0) * t);
            }
            send(viewport, down, down + 96L, MotionEvent.ACTION_UP, x1, y1);
            return null;
        });
        settleLayout();
    }

    private static byte[] readAsset(String name) {
        try (InputStream in = InstrumentationRegistry.getInstrumentation()
                .getContext().getAssets().open(name)) {
            final ByteArrayOutputStream out = new ByteArrayOutputStream();
            final byte[] buffer = new byte[16384];
            int read;
            while ((read = in.read(buffer)) > 0) {
                out.write(buffer, 0, read);
            }
            return out.toByteArray();
        } catch (IOException e) {
            throw new AssertionError("asset " + name + " must be packaged", e);
        }
    }

    private static void writeFile(File file, byte[] bytes) {
        try (FileOutputStream out = new FileOutputStream(file)) {
            out.write(bytes);
        } catch (IOException e) {
            throw new AssertionError("could not write " + file, e);
        }
    }

    private static void send(View target, long downTime, long eventTime, int action, float x,
                             float y) {
        final MotionEvent event = MotionEvent.obtain(downTime, eventTime, action, x, y, 0);
        try {
            target.dispatchTouchEvent(event);
        } finally {
            event.recycle();
        }
    }

    /** Whether a value is an exact integer multiple of the grid step. */
    private static boolean onGrid(double value, double step) {
        return step > 0.0 && Math.abs(value / step - Math.rint(value / step)) < 1e-6;
    }
}
