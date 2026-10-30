package com.forgeshape.app;

import static com.forgeshape.app.SketchTestSupport.drawRectangleAndExtrude;
import static com.forgeshape.app.SketchTestSupport.onNativeStateChanged;
import static com.forgeshape.app.SketchTestSupport.sketchState;
import static com.forgeshape.app.SketchTestSupport.tapWorld;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.pm.ActivityInfo;
import android.net.Uri;
import android.os.SystemClock;
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

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;

/**
 * `CAD-FACE-SUPPORTED-PLACEMENT-R1`: a face-supported CAD Body is exported
 * where it is drawn and cannot be moved on its own, through the real chrome.
 *
 * <p>The domain half -- the export's placement, the typed and gizmo refusals,
 * the producer carrying its dependent, the legacy stored transform -- is
 * proved by the native {@code GLTF_EXPORT}, {@code GIZMO} and {@code CAD_A3}
 * self-tests. What only a device can show is covered here: the dependent is
 * built through New Sketch's spatial chooser by real taps, the Tool Rail
 * offers it no Transform while an independent body keeps one, a finger dragged
 * through the window over it moves nothing, the native doors refuse when
 * called directly, and the GLB written through the app's own export write path
 * puts it on its producer's face, in portrait and landscape at font scale 1.0
 * and 1.5.
 *
 * <p>The producer is a rectangle dragged to about 2 x 2 on XY and extruded 2
 * (world z {@code [0,2]}); the dependent is a rectangle dragged to about 1 x 1
 * on the producer's far cap and extruded 0.5 (world z {@code [2,2.5]}). The
 * footprints are what the drag snapped to on the view-adaptive grid, so they
 * are measured; the placement is what is asserted.
 * No control is located by coordinate; every viewport pixel comes from
 * {@code debugProjectWorld}.
 */
@RunWith(AndroidJUnit4.class)
public final class CadFaceSupportedPlacementTest {

    private static final float TOLERANCE = 1e-4f;

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private ModelingOwnerRig rig;
    private byte[] baselineProject;
    private final File scratch = new File(InstrumentationRegistry.getInstrumentation()
            .getTargetContext().getCacheDir(), "cad-face-supported-placement");

    @Before
    public void setUp() {
        rig = new ModelingOwnerRig(rule.getScenario(), "cad-face-supported-placement",
                "FSP_DEVICE");
        WorkspaceTestSupport.resetToBaselineConstruction(rule.getScenario());
        baselineProject = NativeViewport.encodeProject();
        //noinspection ResultOfMethodCallIgnored
        scratch.mkdirs();
    }

    @After
    public void tearDown() {
        WorkspaceTestSupport.releaseOrientation(rule.getScenario());
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.setGizmoActive(false);
            NativeViewport.supportChooserCancel();
            NativeViewport.sketchCancel();
            NativeViewport.enterConstructionMode();
            if (baselineProject != null) {
                NativeViewport.loadProject(baselineProject);
            }
            return null;
        });
        rig.restore();
    }

    // -----------------------------------------------------------------------
    // E2E-FSP-01: the dependent offers no Transform, refuses every placement
    // door, and exports on its producer's face; a producer move carries it.
    // -----------------------------------------------------------------------

    @Test
    public void aFaceSupportedBodyIsNotTransformableAndExportsOnItsFace() {
        final long producer = extrudeProducer();
        final long dependent = extrudeDependentOnFarCap();

        // The Tool Rail offers Shape and no Transform; no gizmo is up.
        assertFalse("Transform is absent for a face-supported body",
                rig.shown(R.id.tool_rail_place));
        assertTrue("Shape stays: its sketch and extrusion are what it is",
                rig.shown(R.id.tool_rail_shape));
        assertGizmoDown("the dependent at rest");

        final Witness before = witness();

        // A real finger through the window, from the drawn dependent and from
        // its inert stored pivot (the world origin), moves nothing.
        dragThroughWindowFrom(0.0, 0.0, 2.25);
        dragThroughWindowFrom(0.0, 0.0, 0.0);
        assertEquals("a finger over the dependent changes no project fact", before, witness());

        // The native doors, called directly as a bypassed UI would.
        final int typed = onWorkspace(rule.getScenario(), (activity, workspace) ->
                NativeViewport.applyBoxTransform(5.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0));
        assertEquals("a typed write is refused by its own name",
                NativeViewport.APPLY_REJECTED_DERIVED_PLACEMENT, typed);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.setGizmoActive(true);
            return null;
        });
        assertGizmoDown("setGizmoActive(true) is refused over the dependent");
        dragThroughWindowFrom(0.0, 0.0, 2.25);
        dragThroughWindowFrom(0.0, 0.0, 0.0);
        assertEquals("no Undo step, no fingerprint move, no .forge byte", before, witness());

        // The GLB written through the app's own export write path puts the
        // dependent on the producer's far cap.
        // Footprints are what the drag snapped to on the adaptive grid, so
        // they are MEASURED, never assumed; what is asserted is PLACEMENT --
        // the dependent's base on the producer's far cap (z = its depth 2),
        // its extrusion 0.5 above it, centred on the cap and inside it.
        // Before this stage the file put it at z = [0, 0.5] about the origin.
        GlbDocument document = exportThroughApp("on_face.glb");
        final float[] p0 = worldBounds(body(document, producer));
        final float[] d0 = worldBounds(body(document, dependent));
        rig.fact("producer_world_aabb", java.util.Arrays.toString(p0));
        rig.fact("dependent_world_aabb", java.util.Arrays.toString(d0));
        assertCentredOnZ("producer", p0, 0f, 2f);
        assertCentredOnZ("dependent on the far cap", d0, 2f, 2.5f);
        assertTrue("the dependent stands inside the producer's cap "
                        + java.util.Arrays.toString(d0),
                d0[0] >= p0[0] - TOLERANCE && d0[3] <= p0[3] + TOLERANCE
                        && d0[1] >= p0[1] - TOLERANCE && d0[4] <= p0[4] + TOLERANCE);

        // The producer, an independent body, still offers Transform by a real
        // tap, and a typed Apply through the placement editor moves it -- and
        // carries the dependent with it.
        select(producer);
        assertTrue("an independent CAD body keeps Transform", rig.shown(R.id.tool_rail_place));
        rig.touchId(R.id.tool_rail_place);
        assertEquals("the gizmo stands over the producer", 1.0, gizmo()[NativeViewport.GIZMO_ACTIVE],
                0.0);
        final int stepsBeforeMove = NativeViewport.constructionUndoDepth();
        typePlacementX("3");
        assertEquals("one step for the producer's move", stepsBeforeMove + 1,
                NativeViewport.constructionUndoDepth());
        document = exportThroughApp("producer_moved.glb");
        assertBounds("producer moved", body(document, producer), shifted(p0, 3f));
        assertBounds("dependent followed by exactly the producer's move",
                body(document, dependent), shifted(d0, 3f));

        // Selecting the dependent while Transform is held moves the rail to
        // Shape and takes the gizmo down.
        select(dependent);
        assertFalse(rig.shown(R.id.tool_rail_place));
        assertGizmoDown("Transform held, then the dependent selected");

        // Save and reopen: still face-supported, still refused, still on its face.
        final byte[] saved = NativeViewport.encodeProject();
        assertNotNull(saved);
        assertEquals(NativeViewport.PROJECT_OK, NativeViewport.loadProject(saved));
        select(dependent);
        assertTrue(NativeViewport.sceneActiveBodyIsFaceSupportedCad());
        assertEquals(NativeViewport.APPLY_REJECTED_DERIVED_PLACEMENT,
                (int) onWorkspace(rule.getScenario(), (activity, workspace) ->
                        NativeViewport.applyBoxTransform(1.0, 2.0, 3.0, 0.0, 0.0, 0.0,
                                1.0, 1.0, 1.0)));
        assertBounds("after reopen", body(exportThroughApp("reopened.glb"), dependent),
                shifted(d0, 3f));
    }

    // -----------------------------------------------------------------------
    // E2E-FSP-02: the withdrawal holds in portrait and landscape at font
    // scale 1.0 and 1.5, while the producer keeps Transform in each.
    // -----------------------------------------------------------------------

    @Test
    public void theWithdrawalHoldsAcrossOrientationAndFontScale() {
        final long producer = extrudeProducer();
        final long dependent = extrudeDependentOnFarCap();
        try {
            for (String scale : new String[]{"1.0", "1.5"}) {
                setFontScale(scale);
                for (int orientation : new int[]{ActivityInfo.SCREEN_ORIENTATION_PORTRAIT,
                        ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE}) {
                    WorkspaceTestSupport.setOrientation(rule.getScenario(), orientation);
                    settleLayout();
                    SystemClock.sleep(600);
                    settleLayout();
                    final String where = "font " + scale + " orientation " + orientation;

                    select(producer);
                    assertTrue(where + ": the producer offers Transform",
                            rig.shown(R.id.tool_rail_place));
                    rig.touchId(R.id.tool_rail_place);
                    assertEquals(where + ": the gizmo stands over the producer", 1.0,
                            gizmo()[NativeViewport.GIZMO_ACTIVE], 0.0);

                    select(dependent);
                    assertFalse(where + ": no Transform for the dependent",
                            rig.shown(R.id.tool_rail_place));
                    assertTrue(where + ": Shape stays", rig.shown(R.id.tool_rail_shape));
                    assertGizmoDown(where);
                    final Witness before = witness();
                    dragThroughWindowFrom(0.0, 0.0, 2.25);
                    assertEquals(where + ": a real drag changes nothing", before, witness());
                    rig.capture("fsp_font_" + scale.replace('.', '_') + "_" + orientation);
                }
            }
        } finally {
            setFontScale("1.0");
        }
    }

    // -----------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------

    private long extrudeProducer() {
        assertEquals(NativeViewport.CAD_OK, NativeViewport.sketchBegin(NativeViewport.WORKPLANE_XY));
        onNativeStateChanged(rule.getScenario());
        final long producer = drawRectangleAndExtrude(rule.getScenario(), 2.0, 2.0, "2.0");
        assertFalse(NativeViewport.sceneActiveBodyIsFaceSupportedCad());
        return producer;
    }

    /** New Sketch -> spatial chooser -> real taps on the far cap -> extrude. */
    private long extrudeDependentOnFarCap() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openAddPrimitive(workspace);
            final View newSketch = workspace.addPrimitivePalette().findViewById(R.id.add_sketch);
            assertNotNull(newSketch);
            newSketch.performClick();
            return null;
        });
        settleLayout();
        assertTrue(NativeViewport.supportChooserActive());
        tapWorld(rule.getScenario(), 0.0, 0.0, 2.0);
        assertEquals("the tap aims at a CAD face", 3, NativeViewport.supportChooserSelectedKind());
        tapWorld(rule.getScenario(), 0.0, 0.0, 2.0);
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
        final long dependent = drawRectangleAndExtrude(rule.getScenario(), 1.0, 1.0, "0.5");
        assertTrue("the new body is face-supported",
                NativeViewport.sceneActiveBodyIsFaceSupportedCad());
        return dependent;
    }

    private void select(long id) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(id));
            workspace.syncFromNative();
            return null;
        });
        settleLayout();
    }

    // -----------------------------------------------------------------------
    // Input
    // -----------------------------------------------------------------------

    /** A real finger through the window, from a world point, 200 px across. */
    private void dragThroughWindowFrom(double x, double y, double z) {
        final float[] at = new float[2];
        assertTrue("the start point projects", NativeViewport.debugProjectWorld(x, y, z, at));
        rig.realDrag(at[0], at[1], at[0] + 200f, at[1] + 40f, 10);
    }

    /** Types X into the placement editor and presses Apply by a real tap. */
    private void typePlacementX(String value) {
        if (!rig.shown(R.id.apply_transform)) {
            rig.touchId(R.id.precision_toggle);
        }
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final EditText field = workspace.placementEditor().findViewById(R.id.field_pos_x);
            assertNotNull(field);
            field.setText(value);
            return null;
        });
        settleLayout();
        rig.touchId(R.id.apply_transform);
    }

    private void setFontScale(String scale) {
        shell("settings put system font_scale " + scale);
        final float want = Float.parseFloat(scale);
        boolean applied = false;
        for (int attempt = 0; attempt < 60 && !applied; attempt++) {
            final Float now = onWorkspace(rule.getScenario(), (activity, workspace) ->
                    activity.getResources().getConfiguration().fontScale);
            applied = now != null && Math.abs(now - want) < 0.01f;
            if (!applied) SystemClock.sleep(100);
        }
        assertTrue("font scale " + scale + " took effect", applied);
        settleLayout();
        SystemClock.sleep(400);
        settleLayout();
        rig.fact("font_scale", scale);
    }

    private static void shell(String command) {
        final android.os.ParcelFileDescriptor pipe = InstrumentationRegistry
                .getInstrumentation().getUiAutomation().executeShellCommand(command);
        try (InputStream stream = new android.os.ParcelFileDescriptor.AutoCloseInputStream(pipe)) {
            final byte[] drain = new byte[64];
            while (stream.read(drain) >= 0) {
                // Drained so the command completes before the case continues.
            }
        } catch (IOException ignored) {
            // The command still ran; nothing here reads its output.
        }
        SystemClock.sleep(200);
    }

    // -----------------------------------------------------------------------
    // Facts
    // -----------------------------------------------------------------------

    private static double[] gizmo() {
        final double[] state = new double[NativeViewport.GIZMO_STATE_SIZE];
        NativeViewport.gizmoState(state);
        return state;
    }

    private void assertGizmoDown(String where) {
        final double[] state = gizmo();
        assertEquals(where + ": gizmo inactive", 0.0, state[NativeViewport.GIZMO_ACTIVE], 0.0);
        assertEquals(where + ": gizmo not drawn", 0.0, state[NativeViewport.GIZMO_VISIBLE], 0.0);
    }

    /** Everything a placement write would move: history, fingerprint, bytes. */
    private static final class Witness {
        final int undo;
        final int redo;
        final long fingerprint;
        final byte[] document;

        Witness(int undo, int redo, long fingerprint, byte[] document) {
            this.undo = undo;
            this.redo = redo;
            this.fingerprint = fingerprint;
            this.document = document;
        }

        @Override
        public boolean equals(Object other) {
            if (!(other instanceof Witness)) return false;
            final Witness w = (Witness) other;
            return undo == w.undo && redo == w.redo && fingerprint == w.fingerprint
                    && java.util.Arrays.equals(document, w.document);
        }

        @Override
        public int hashCode() {
            return Long.hashCode(fingerprint);
        }

        @Override
        public String toString() {
            return "undo=" + undo + " redo=" + redo + " fingerprint=" + fingerprint
                    + " bytes=" + (document == null ? -1 : document.length);
        }
    }

    private static Witness witness() {
        return new Witness(NativeViewport.constructionUndoDepth(),
                NativeViewport.constructionRedoDepth(), NativeViewport.projectFingerprint(),
                NativeViewport.encodeProject());
    }

    // -----------------------------------------------------------------------
    // Export
    // -----------------------------------------------------------------------

    /** The bytes the exporter produces, written where the user chose, read back. */
    private GlbDocument exportThroughApp(String name) {
        final byte[] bytes = NativeViewport.exportGlb();
        assertNotNull("the export succeeds", bytes);
        final File destination = new File(scratch, name);
        //noinspection ResultOfMethodCallIgnored
        destination.delete();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onExportGlbRequestedForTest(bytes);
            workspace.onCreateGlbDocumentChosen(Uri.fromFile(destination));
            return null;
        });
        settleLayout();
        assertTrue("the file was written", destination.isFile());
        final byte[] written = readFile(destination);
        assertArrayEquals("byte for byte what the exporter produced", bytes, written);
        final GlbDocument document = GlbDocument.parse(written);
        assertEquals("a valid GLB", 0, document.validate().size());
        rig.fact("export_" + name + "_bytes", written.length);
        return document;
    }

    private static GlbDocument.Body body(GlbDocument document, long id) {
        for (GlbDocument.Body body : document.bodies()) {
            if (("Body_" + id).equals(body.name)) {
                return body;
            }
        }
        throw new AssertionError("no node named Body_" + id);
    }

    /** {@code [minX, minY, minZ, maxX, maxY, maxZ]} of the body's world vertices. */
    private static float[] worldBounds(GlbDocument.Body body) {
        final float[] lo = {Float.MAX_VALUE, Float.MAX_VALUE, Float.MAX_VALUE};
        final float[] hi = {-Float.MAX_VALUE, -Float.MAX_VALUE, -Float.MAX_VALUE};
        final int count = body.positions.length / 3;
        assertNotEquals(body.name + " has geometry", 0, count);
        for (int i = 0; i < count; i++) {
            final float[] p = body.worldVertex(i);
            for (int axis = 0; axis < 3; axis++) {
                lo[axis] = Math.min(lo[axis], p[axis]);
                hi[axis] = Math.max(hi[axis], p[axis]);
            }
        }
        return new float[]{lo[0], lo[1], lo[2], hi[0], hi[1], hi[2]};
    }

    private static float[] shifted(float[] box, float dx) {
        return new float[]{box[0] + dx, box[1], box[2], box[3] + dx, box[4], box[5]};
    }

    /** z spans exactly [z0, z1]; x and y are a real, origin-centred footprint. */
    private static void assertCentredOnZ(String what, float[] box, float z0, float z1) {
        final String got = what + " " + java.util.Arrays.toString(box);
        assertEquals(got + ": bottom", z0, box[2], TOLERANCE);
        assertEquals(got + ": top", z1, box[5], TOLERANCE);
        assertTrue(got + ": a real footprint", box[3] - box[0] > 0.1f && box[4] - box[1] > 0.1f);
        assertEquals(got + ": centred in x", 0f, box[0] + box[3], 2 * TOLERANCE);
        assertEquals(got + ": centred in y", 0f, box[1] + box[4], 2 * TOLERANCE);
    }

    private static void assertBounds(String what, GlbDocument.Body body, float[] want) {
        final float[] got = worldBounds(body);
        for (int i = 0; i < 6; i++) {
            assertEquals(what + ": world AABB[" + i + "] " + java.util.Arrays.toString(got),
                    want[i], got[i], TOLERANCE);
        }
    }

    private static byte[] readFile(File file) {
        try (FileInputStream in = new FileInputStream(file)) {
            final ByteArrayOutputStream out = new ByteArrayOutputStream();
            final byte[] buffer = new byte[8192];
            int read;
            while ((read = in.read(buffer)) >= 0) {
                out.write(buffer, 0, read);
            }
            return out.toByteArray();
        } catch (IOException e) {
            throw new AssertionError("reading " + file, e);
        }
    }
}
