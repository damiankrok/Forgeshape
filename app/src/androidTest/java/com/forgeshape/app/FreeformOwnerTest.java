package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Freeform/SubD on the device ({@code MODELING-FOUNDATIONS-R1} B), on the
 * OWNER's own path: Home → New Project → Freeform lands on a Freeform Box with
 * its cage open under Shape. Every act a user performs is a REAL window touch:
 * the New Project tile, the precision toggle, the context surface's chips and
 * tools, a tap on a cage vertex, edge or face at its projected pixel, a drag
 * of the cage gizmo's handle, and Undo/Redo in the history capsule.
 *
 * <p>Asserted from native truth -- the cage (ids, counts, binary64 positions),
 * the Construction history depth, the derived-surface digest, the encoded
 * project -- never from pixels. Screenshots are evidence only.
 */
@RunWith(AndroidJUnit4.class)
public final class FreeformOwnerTest {

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private ModelingOwnerRig rig;
    private final double[] slots = new double[NativeViewport.FREEFORM_STATE_SIZE];

    @Before
    public void startAtHome() {
        rig = new ModelingOwnerRig(rule.getScenario(), "modeling-freeform", "MFFF_DEVICE");
        rig.startAtHome();
    }

    @After
    public void restoreAProject() {
        rig.restore();
    }

    // =======================================================================
    // DEV-FF-01: New Project → Freeform; a vertex tapped on the cage and moved
    // by a real drag of the cage gizmo is ONE Undo, and Redo puts it back.
    // =======================================================================

    @Test
    public void devFf01_new_freeform_box_vertex_tap_and_gizmo_drag_is_one_undo() {
        newFreeformProject();
        assertEquals("a Freeform Box: 8 vertices", 8, (int) state(NativeViewport.FREEFORM_STATE_VERTICES));
        assertEquals(12, (int) state(NativeViewport.FREEFORM_STATE_EDGES));
        assertEquals(6, (int) state(NativeViewport.FREEFORM_STATE_FACES));
        assertEquals("the new project's history is empty", 0, NativeViewport.constructionUndoDepth());
        rig.capture("01_freeform_box");

        chooseElement(R.id.freeform_element_vertex, NativeViewport.FREEFORM_ELEMENT_VERTEX);
        tapElement(NativeViewport.FREEFORM_ELEMENT_VERTEX, 7);
        assertArrayEquals("the tap selected vertex 7 by id", new int[]{7}, selection());

        final float[] pivot = screenPoint(NativeViewport.FREEFORM_ELEMENT_VERTEX, 7);
        final float[] handle = new float[2];
        assertTrue("the cage gizmo's X handle projects",
                NativeViewport.freeformGizmoHandlePoint(NativeViewport.GIZMO_HANDLE_AXIS_X, handle));
        final float dx = handle[0] - pivot[0];
        final float dy = handle[1] - pivot[1];
        final float length = (float) Math.hypot(dx, dy);
        assertTrue("the handle has a screen direction", length > 8f);
        final float reach = 140f / length;
        final float[] end = {handle[0] + dx * reach, handle[1] + dy * reach};
        assertTrue(rig.viewportPointFree(handle[0], handle[1]));
        assertTrue(rig.viewportPointFree(end[0], end[1]));
        final int depthBefore = NativeViewport.constructionUndoDepth();
        rig.realDrag(handle[0], handle[1], end[0], end[1], 14);
        final double[] moved = vertex(7);
        rig.fact("ff01.moved", moved[0] + "," + moved[1] + "," + moved[2]);
        assertTrue("the drag moved vertex 7 along +X", moved[0] > 0.55);
        assertEquals("Y untouched", 0.5, moved[1], 0.0);
        assertEquals("Z untouched", 0.5, moved[2], 0.0);
        assertEquals("one drag is one step", depthBefore + 1, NativeViewport.constructionUndoDepth());
        assertEquals("only the cage moved", 0.5, vertex(3)[0], 0.0);
        rig.capture("02_vertex_dragged");

        rig.touchId(R.id.undo_action);
        assertEquals("Undo restores the exact cage", 0.5, vertex(7)[0], 0.0);
        rig.touchId(R.id.redo_action);
        assertEquals("Redo restores the drag exactly", moved[0], vertex(7)[0], 0.0);
    }

    // =======================================================================
    // DEV-FF-02: Push/Pull a face by an exact typed distance, insert an edge
    // loop, crease an edge and extrude a face -- each one Undo.
    // =======================================================================

    @Test
    public void devFf02_push_pull_insert_loop_crease_and_extrude_are_one_undo_each() {
        newFreeformProject();
        chooseElement(R.id.freeform_element_face, NativeViewport.FREEFORM_ELEMENT_FACE);
        tapPoint(screenPoint(NativeViewport.FREEFORM_ELEMENT_FACE, 4));
        assertArrayEquals("the tap selected the +Y face by id", new int[]{4}, selection());

        int depth = NativeViewport.constructionUndoDepth();
        openPrecision();
        typeField(R.id.field_freeform_distance, "0.25");
        rig.touchId(R.id.freeform_push_pull);
        assertEquals("Push/Pull moved the face exactly 0.25 m", 0.75, vertex(3)[1], 0.0);
        assertEquals(0.75, vertex(8)[1], 0.0);
        assertEquals(++depth, NativeViewport.constructionUndoDepth());

        rig.touchId(R.id.freeform_element_edge);
        closePrecision();
        tapPoint(screenPoint(NativeViewport.FREEFORM_ELEMENT_EDGE, 7));
        assertArrayEquals("the tap selected edge 7 by id", new int[]{7}, selection());
        openPrecision();
        typeField(R.id.field_freeform_ratio, "0.5");
        rig.touchId(R.id.freeform_insert_loop);
        assertEquals("a loop across the whole ring: 4 new vertices", 12,
                (int) state(NativeViewport.FREEFORM_STATE_VERTICES));
        assertEquals(10, (int) state(NativeViewport.FREEFORM_STATE_FACES));
        assertEquals("the loop stands at the ring's midpoint", 0.0, vertex(9)[0], 0.0);
        assertEquals(++depth, NativeViewport.constructionUndoDepth());

        final long smooth = NativeViewport.freeformMeshDigest();
        final double[] before = vertex(7);
        typeField(R.id.field_freeform_crease, "1");
        rig.touchId(R.id.freeform_set_crease);
        assertNotEquals("a crease changes the derived surface", smooth, NativeViewport.freeformMeshDigest());
        assertArrayEquals("and no control vertex", before, vertex(7), 0.0);
        assertEquals(++depth, NativeViewport.constructionUndoDepth());
        rig.capture("03_loop_and_crease");

        rig.touchId(R.id.freeform_element_face);
        closePrecision();
        tapPoint(screenPoint(NativeViewport.FREEFORM_ELEMENT_FACE, 4));
        assertArrayEquals(new int[]{4}, selection());
        final int faces = (int) state(NativeViewport.FREEFORM_STATE_FACES);
        openPrecision();
        typeField(R.id.field_freeform_distance, "0.5");
        rig.touchId(R.id.freeform_extrude);
        assertEquals("one wall per boundary edge of the face", faces + 4,
                (int) state(NativeViewport.FREEFORM_STATE_FACES));
        assertEquals(++depth, NativeViewport.constructionUndoDepth());
        rig.capture("04_extruded");

        rig.touchId(R.id.undo_action);
        assertEquals("Undo takes back the extrusion alone", faces,
                (int) state(NativeViewport.FREEFORM_STATE_FACES));
        rig.touchId(R.id.redo_action);
        assertEquals(faces + 4, (int) state(NativeViewport.FREEFORM_STATE_FACES));
    }

    // =======================================================================
    // DEV-FF-03: symmetry mirrors an edit exactly; level 0 vs 3 changes the
    // surface; save and reopen keep the exact cage and ids.
    // =======================================================================

    @Test
    public void devFf03_symmetry_subdivision_and_save_reopen_keep_the_exact_cage() {
        newFreeformProject();
        openPrecision();
        rig.touchId(R.id.freeform_symmetry_x);
        assertEquals("symmetry about x = 0", NativeViewport.FREEFORM_SYMMETRY_X,
                (int) state(NativeViewport.FREEFORM_STATE_SYMMETRY));
        closePrecision();
        tapPoint(screenPoint(NativeViewport.FREEFORM_ELEMENT_FACE, 6));
        assertArrayEquals("the +X face", new int[]{6}, selection());
        openPrecision();
        typeField(R.id.field_freeform_distance, "0.25");
        rig.touchId(R.id.freeform_push_pull);
        assertEquals(0.75, vertex(2)[0], 0.0);
        assertEquals("the mirror face moved exactly with it", -0.75, vertex(1)[0], 0.0);

        rig.touchId(R.id.freeform_level_0);
        assertEquals(0, (int) state(NativeViewport.FREEFORM_STATE_LEVEL));
        final long level0 = NativeViewport.freeformMeshDigest();
        rig.capture("05_level_0");
        rig.touchId(R.id.freeform_level_3);
        assertEquals(3, (int) state(NativeViewport.FREEFORM_STATE_LEVEL));
        assertEquals("level 3: 64 derived quads per control face", 6 * 64,
                (int) state(NativeViewport.FREEFORM_STATE_DERIVED_QUADS));
        assertNotEquals("level 3 is a different, smoother surface", level0,
                NativeViewport.freeformMeshDigest());
        rig.capture("06_level_3");

        final byte[] saved = NativeViewport.encodeProject();
        assertNotNull(saved);
        final long fingerprint = NativeViewport.projectFingerprint();
        final long digest = NativeViewport.freeformMeshDigest();
        final double[][] cage = new double[9][];
        for (int id = 1; id <= 8; id++) {
            cage[id] = vertex(id);
        }
        assertEquals(NativeViewport.PROJECT_OK, rig.applyOnUi(() -> NativeViewport.loadProject(saved)));
        assertEquals("the reopened body is Freeform", NativeViewport.REPRESENTATION_FREEFORM,
                NativeViewport.sceneBodyRepresentation(NativeViewport.sceneActiveBodyId()));
        for (int id = 1; id <= 8; id++) {
            assertArrayEquals("vertex " + id + " reopens bit for bit", cage[id], vertex(id), 0.0);
        }
        assertEquals(NativeViewport.FREEFORM_SYMMETRY_X, (int) state(NativeViewport.FREEFORM_STATE_SYMMETRY));
        assertEquals(3, (int) state(NativeViewport.FREEFORM_STATE_LEVEL));
        assertEquals("the same surface", digest, NativeViewport.freeformMeshDigest());
        assertEquals(fingerprint, NativeViewport.projectFingerprint());
        assertArrayEquals("reopening wrote nothing new", saved, NativeViewport.encodeProject());
        rig.fact("ff03.bytes", saved.length);
    }

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    private void newFreeformProject() {
        rig.touchId(R.id.home_new_project);
        rig.touchId(R.id.new_project_freeform);
        assertTrue("Freeform created the project", NativeViewport.projectOpen());
        assertEquals(NativeViewport.REPRESENTATION_FREEFORM,
                NativeViewport.sceneBodyRepresentation(NativeViewport.sceneActiveBodyId()));
        assertEquals("the cage is open under Shape", 1, (int) state(NativeViewport.FREEFORM_STATE_ACTIVE));
        assertTrue(NativeViewport.debugSetCameraPose(0.6f, 0.45f, 4.5f));
        rig.refresh();
    }

    private void chooseElement(int chipId, int element) {
        openPrecision();
        rig.touchId(chipId);
        assertEquals(element, (int) state(NativeViewport.FREEFORM_STATE_ELEMENT));
        closePrecision();
    }

    private void openPrecision() {
        final boolean open = rig.on((activity, workspace) -> workspace.propertyInspector().isOpen());
        if (!open) {
            rig.touchId(R.id.precision_toggle);
        }
        assertTrue("the cage context surface is on screen", rig.shown(R.id.freeform_editor));
    }

    private void closePrecision() {
        final boolean open = rig.on((activity, workspace) -> workspace.propertyInspector().isOpen());
        if (open) {
            rig.touchId(R.id.precision_toggle);
        }
    }

    private void typeField(final int fieldId, final String text) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final NumericPropertyRow row = workspace.freeformEditor().rowFor(fieldId);
            assertNotNull(row);
            row.setText(text);
            return null;
        });
        settleLayout();
    }

    private void tapElement(int element, int id) {
        tapPoint(screenPoint(element, id));
    }

    private void tapPoint(float[] at) {
        assertTrue("the cage point is on the viewport, under no chrome",
                rig.viewportPointFree(at[0], at[1]));
        rig.realGesture(new float[][]{{at[0], at[1]}, {at[0], at[1]}});
    }

    private float[] screenPoint(int element, int id) {
        final float[] at = new float[2];
        assertTrue("element " + id + " projects", NativeViewport.freeformElementScreenPoint(element, id, at));
        return at;
    }

    private double state(int slot) {
        NativeViewport.freeformState(slots);
        return slots[slot];
    }

    private int[] selection() {
        final int[] ids = new int[64];
        final int count = NativeViewport.freeformSelection(ids);
        final int[] out = new int[count];
        System.arraycopy(ids, 0, out, 0, count);
        return out;
    }

    private static double[] vertex(int id) {
        final double[] xyz = new double[3];
        assertTrue("vertex " + id + " exists", NativeViewport.freeformVertexPosition(id, xyz));
        return xyz;
    }
}
