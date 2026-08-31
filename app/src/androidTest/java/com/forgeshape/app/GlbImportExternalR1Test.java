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
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.json.JSONArray;
import org.json.JSONObject;
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
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.Arrays;
import java.util.Locale;

/**
 * `GLBIR1-16..21` and `E2E-GLBIR1-01..09`: the widened external-GLB preview.
 *
 * <h2>What R1 changed, and what it did not</h2>
 *
 * <p>The reader now accepts the class of static file another sculpting tool
 * writes: a node matrix or TRS, several TRIANGLES primitives over one shared
 * POSITION accessor, a missing NORMAL, ignored colour and UV attributes, and a
 * material's {@code doubleSided}. What did NOT change is everything the preview
 * is: no {@code ObjectId}, no Construction Source, no sculpt representation, no
 * history step, no `.forge` byte, no checkpoint, not selectable, not editable,
 * not re-exportable, and gone with the process. Most of the cases below exist
 * to hold that line while the parser gets more permissive.
 *
 * <h2>The compatibility target</h2>
 *
 * <p>The owner's own low-poly character is external to this repository, so the
 * file exercised here is a synthetic one that carries the same STRUCTURAL
 * features — see {@code forgeshape_glb_import_fixture.h}. It is not the owner's
 * asset and is never described as one. Opening the real file stays the owner's
 * manual step.
 */
@RunWith(AndroidJUnit4.class)
public final class GlbImportExternalR1Test {

    /** What the fixture builder states about itself, asserted rather than assumed. */
    private static final int FIXTURE_PRIMITIVES = 7;
    private static final int FIXTURE_VERTICES = 43 * 46;
    private static final int FIXTURE_TRIANGLES = 42 * 45 * 2;

    private static final String FORGE_CONSTRUCTION = "forge/construction_multibody_v1.forge";

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private final File scratch = new File(context().getCacheDir(), "glb-import-r1-test");

    private static Context context() {
        return InstrumentationRegistry.getInstrumentation().getTargetContext();
    }

    @Before
    public void setUp() {
        context().deleteFile(ProjectSlot.SLOT_FILE_NAME);
        context().deleteFile(ProjectCheckpoint.CHECKPOINT_FILE_NAME);
        deleteRecursively(scratch);
        assertTrue(scratch.mkdirs());
        clearPreview();
        resetToBaselineConstruction(rule.getScenario());
    }

    @After
    public void tearDown() {
        clearPreview();
        deleteRecursively(scratch);
        context().deleteFile(ProjectSlot.SLOT_FILE_NAME);
        context().deleteFile(ProjectCheckpoint.CHECKPOINT_FILE_NAME);
        resetToBaselineConstruction(rule.getScenario());
    }

    // -----------------------------------------------------------------------
    // GLBIR1-19 / E2E-GLBIR1-01: the Nomad-like fixture opens and draws
    // -----------------------------------------------------------------------

    @Test
    public void glbir1_19_theNomadLikeFixtureImportsThroughTheRealResultSeam() {
        final byte[] fixture = fixtureBytes();
        openAsGlbDocument(fixture);

        assertTrue("the preview holds the file", loaded());
        assertTrue("and a successful import shows it", previewVisible());

        final int[] counts = previewCounts();
        assertEquals("one node, one mesh", 1, counts[0]);
        assertEquals("a shared POSITION accessor is decoded once, not seven times",
                FIXTURE_VERTICES, counts[1]);
        assertEquals("every triangle of every primitive survives", FIXTURE_TRIANGLES, counts[2]);
        assertEquals("one draw batch per TRIANGLES primitive", FIXTURE_PRIMITIVES, counts[3]);
    }

    /**
     * E2E-GLBIR1-02. The fixture carries no NORMAL, so every normal reaching
     * the preview was generated — and none of it is NaN or infinite, which is
     * what a bad generation looks like on screen.
     */
    @Test
    public void e2eGlbir1_02_aFileWithNoNormalStillDrawsFiniteGeometry() {
        openAsGlbDocument(fixtureBytes());
        final float[] bounds = previewBounds();
        for (float value : bounds) {
            assertTrue("every bound must be finite, was " + Arrays.toString(bounds),
                    !Float.isNaN(value) && !Float.isInfinite(value));
        }
        assertTrue("the preview has a volume", bounds[3] > bounds[0] && bounds[4] > bounds[1]
                && bounds[5] > bounds[2]);
        settleFrames();
        assertTrue("and it is still shown after several frames", previewVisible());
    }

    /**
     * E2E-GLBIR1-03. The node matrix is baked, column-major, and the world
     * bounds prove it: they are computed here from the file's own bytes by an
     * independent little reader, then compared with what the product produced.
     * A transposed matrix, a dropped one or a mirrored bake all move them.
     */
    @Test
    public void e2eGlbir1_03_theMatrixBakedGeometryHasTheExpectedWorldBounds() {
        final byte[] fixture = fixtureBytes();
        openAsGlbDocument(fixture);

        final float[] expected = expectedWorldBounds(fixture);
        final float[] actual = previewBounds();
        for (int axis = 0; axis < 6; ++axis) {
            assertEquals("world bound " + axis + " expected " + Arrays.toString(expected)
                            + " actual " + Arrays.toString(actual),
                    expected[axis], actual[axis], 1e-4f);
        }
        // Asymmetric on purpose: if the surface were symmetric these bounds
        // could not tell a right matrix from a wrong one.
        assertNotEquals("the fixture must not be symmetric about Y",
                Math.abs(expected[1]), Math.abs(expected[4]), 1e-3f);
    }

    /**
     * E2E-GLBIR1-04. The fixture's one material is double-sided, so the
     * preview must not cull its back faces. Asserted at the seam that decides
     * it — the published mesh's own two-sided flag — because a screenshot of a
     * flat-shaded grey surface cannot tell the two apart reliably.
     */
    @Test
    public void e2eGlbir1_04_aDoubleSidedMaterialIsDrawnFromBothSides() {
        openAsGlbDocument(fixtureBytes());
        assertTrue("every batch of a doubleSided material renders both sides",
                onWorkspace(rule.getScenario(),
                        (activity, workspace) -> NativeViewport.debugPreviewRendersBothSides()));
    }

    /**
     * E2E-GLBIR1-05. Navigation keeps working over a preview, driven by a REAL
     * one-finger orbit through the platform-neutral input seam rather than by
     * setting a pose. The preview is what the viewport DRAWS and says nothing
     * about how it is looked at.
     */
    @Test
    public void e2eGlbir1_05_viewportNavigationStillWorksOverAPreview() {
        openAsGlbDocument(fixtureBytes());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(NativeViewport.debugSetCameraPose(0.6f, 0.35f, 9.0f));
            return null;
        });
        settleFrames();
        final float[] before = cameraPose();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int width = workspace.getWidth();
            final int height = workspace.getHeight();
            final float x = width * 0.5f;
            final float y = height * 0.5f;
            sendTouch(android.view.MotionEvent.ACTION_DOWN, x, y, width, height);
            for (int step = 1; step <= 8; ++step) {
                sendTouch(android.view.MotionEvent.ACTION_MOVE, x + step * 12f, y + step * 4f,
                        width, height);
            }
            sendTouch(android.view.MotionEvent.ACTION_UP, x + 96f, y + 32f, width, height);
            return null;
        });
        settleFrames();

        final float[] after = cameraPose();
        assertNotEquals("an orbit must have turned the camera", before[0], after[0], 1e-4f);
        assertTrue("and the preview is still what the viewport draws", previewVisible());
        assertTrue("with its geometry intact", loaded());
    }

    private static void sendTouch(int action, float x, float y, int width, int height) {
        // A plain finger: null stylus arrays, exactly as the product's own
        // handler passes for hardware that reports no pressure or tilt.
        NativeViewport.touchEvent(action, -1, 1, new int[]{0}, new float[]{x},
                new float[]{y}, null, null, null, null, width, height);
    }

    private float[] cameraPose() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final float[] pose = new float[NativeViewport.CAMERA_POSE_SIZE];
            NativeViewport.debugCameraPose(pose);
            return pose;
        });
    }

    // -----------------------------------------------------------------------
    // GLBIR1-17/18 and E2E-GLBIR1-06/09: the preview owns nothing
    // -----------------------------------------------------------------------

    /**
     * GLBIR1-17 / E2E-GLBIR1-06. Source to Imported and back leaves the
     * project snapshot, the history, the manual slot and the autosave
     * fingerprint bit-identical. A widened parser must not widen what the
     * preview touches, which is nothing.
     */
    @Test
    public void glbir1_17_theWidenedPreviewStillOwnsNoProjectState() {
        openForgeSample(FORGE_CONSTRUCTION);
        final byte[] projectBefore = encodeProject();
        final long fingerprintBefore = fingerprint();
        final long[] bodiesBefore = sceneBodyIds();
        final long activeBefore = activeBodyId();
        final long revisionBefore = meshRevision();
        final int undoBefore = undoDepth();
        final int redoBefore = redoDepth();

        openAsGlbDocument(fixtureBytes());
        assertTrue(previewVisible());
        // The preview added no body and stole no identity: its renderer keys
        // come from a reserved range the scene's allocator cannot reach.
        assertArrayEquals("no body appeared", bodiesBefore, sceneBodyIds());
        assertEquals("the active body did not change", activeBefore, activeBodyId());
        assertEquals("no mesh revision was published", revisionBefore, meshRevision());
        assertEquals("no history step was recorded", undoBefore, undoDepth());
        assertEquals("and the redo stack was left alone", redoBefore, redoDepth());
        assertArrayEquals("the project bytes are unchanged", projectBefore, encodeProject());
        assertEquals("and so is the autosave fingerprint", fingerprintBefore, fingerprint());

        // Back to the model, then Clear. Still identical.
        setPreviewVisible(false);
        assertFalse(previewVisible());
        assertArrayEquals(projectBefore, encodeProject());
        clearPreview();
        assertFalse(loaded());
        assertArrayEquals(projectBefore, encodeProject());
        assertEquals(fingerprintBefore, fingerprint());
        assertArrayEquals(bodiesBefore, sceneBodyIds());

        // E2E-GLBIR1-09: and the source is still editable afterwards.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionBox(1.5, 0.75, 0.25);
            return null;
        });
        settleLayout();
        assertNotEquals("the project must still be editable after a preview",
                fingerprintBefore, fingerprint());
    }

    /**
     * GLBIR1-17. Exporting while a preview is shown exports the PROJECT. The
     * preview is not in the scene and cannot reach a `.glb` ForgeShape writes.
     */
    @Test
    public void glbir1_17_aPreviewIsNeverReExportedAsProjectContent() {
        openForgeSample(FORGE_CONSTRUCTION);
        final byte[] exportedBefore = exportGlb();
        openAsGlbDocument(fixtureBytes());
        assertTrue(previewVisible());
        assertArrayEquals("the export is the project, preview or no preview",
                exportedBefore, exportGlb());
    }

    /**
     * E2E-GLBIR1-07. A file the reader will not take changes nothing at all —
     * no preview, no residue, no project movement — and says why in a bounded
     * category, with the precise token in the log.
     */
    @Test
    public void e2eGlbir1_07_anUnsupportedExternalFileRefusesWithoutResidue() {
        openForgeSample(FORGE_CONSTRUCTION);
        final byte[] projectBefore = encodeProject();

        // A GLB the reader understands well enough to refuse precisely: a
        // required compression extension it implements none of.
        final byte[] compressed = withRequiredExtension(fixtureBytes());
        final int status = importPreview(compressed);
        assertNotEquals(NativeViewport.IMPORT_OK, status);
        assertEquals("UnsupportedExtension", statusToken(status));
        assertEquals("a valid glTF feature outside the subset is 'unsupported'",
                NativeViewport.IMPORT_CATEGORY_UNSUPPORTED, statusCategory(status));
        assertFalse("nothing was loaded", loaded());
        assertFalse("and nothing is shown", previewVisible());
        assertArrayEquals(projectBefore, encodeProject());

        // And a file that is not a GLB at all: a `.forge` document, which must
        // never reach the GLB parser as anything but a refusal.
        final int notGlb = importPreview(readAsset(FORGE_CONSTRUCTION));
        assertNotEquals(NativeViewport.IMPORT_OK, notGlb);
        assertEquals("NotGlb", statusToken(notGlb));
        assertEquals(NativeViewport.IMPORT_CATEGORY_UNREADABLE, statusCategory(notGlb));
        assertFalse(loaded());
        assertArrayEquals(projectBefore, encodeProject());
    }

    /**
     * A refusal never replaces a preview that is already there. The widened
     * parser has more ways to refuse, so this matters more than it did.
     */
    @Test
    public void glbir1_17_aRefusalLeavesAnExistingPreviewWhole() {
        openAsGlbDocument(fixtureBytes());
        final int[] before = previewCounts();
        assertNotEquals(NativeViewport.IMPORT_OK,
                importPreview(withRequiredExtension(fixtureBytes())));
        assertTrue("the previous preview survived", loaded());
        assertArrayEquals(before, previewCounts());
    }

    // -----------------------------------------------------------------------
    // GLBIR1-16 / E2E-GLBIR1-08: Import GLB and Open Project are two acts
    // -----------------------------------------------------------------------

    @Test
    public void glbir1_16_importGlbAndOpenProjectAreDistinctActions() {
        final Intent glb = ProjectTransfer.openGlbDocumentIntent();
        final Intent project = ProjectTransfer.openDocumentIntent();

        assertEquals(Intent.ACTION_OPEN_DOCUMENT, glb.getAction());
        assertTrue("the picker must offer openable documents",
                glb.getCategories().contains(Intent.CATEGORY_OPENABLE));
        final String[] glbTypes = glb.getStringArrayExtra(Intent.EXTRA_MIME_TYPES);
        assertNotNull("the GLB picker states its types", glbTypes);
        assertTrue("model/gltf-binary is offered where a provider reports it",
                Arrays.asList(glbTypes).contains(ProjectTransfer.GLB_MIME_TYPE));
        assertTrue("with a fallback, because providers report `.glb` as anything",
                glbTypes.length > 1);

        assertEquals(Intent.ACTION_OPEN_DOCUMENT, project.getAction());
        final String[] projectTypes = project.getStringArrayExtra(Intent.EXTRA_MIME_TYPES);
        assertFalse("the project picker must not advertise a GLB type",
                projectTypes != null
                        && Arrays.asList(projectTypes).contains(ProjectTransfer.GLB_MIME_TYPE));

        // E2E-GLBIR1-08: neither file ever reaches the other's decoder. A
        // `.glb` handed to the project path is refused and the live project
        // stays exactly what it was.
        openForgeSample(FORGE_CONSTRUCTION);
        final byte[] projectBefore = encodeProject();
        final File glbFile = new File(scratch, "external.glb");
        writeFile(glbFile, fixtureBytes());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onOpenProjectDocumentChosen(Uri.fromFile(glbFile));
            return null;
        });
        settleLayout();
        assertArrayEquals("a `.glb` opened as a project changes nothing",
                projectBefore, encodeProject());
        assertFalse("and it certainly does not become a preview", loaded());
    }

    /** GLBIR1-16. Cancelling the GLB picker is a no-op in every respect. */
    @Test
    public void glbir1_16_cancellingTheImportPickerChangesNothing() {
        openForgeSample(FORGE_CONSTRUCTION);
        final byte[] projectBefore = encodeProject();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onOpenGlbDocumentChosen(null);
            return null;
        });
        settleLayout();
        assertFalse(loaded());
        assertArrayEquals(projectBefore, encodeProject());
    }

    // -----------------------------------------------------------------------
    // GLBIR1-20: the controls, unchanged in geometry and named for the act
    // -----------------------------------------------------------------------

    @Test
    public void glbir1_20_theImportControlsKeepTheirIdsAndTheTouchFloor() {
        openProjectSurface();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View importRow = workspace.findViewById(R.id.import_glb_preview);
            assertNotNull("the import control exists by id", importRow);
            assertEquals("Import GLB…",
                    ((android.widget.TextView) importRow).getText().toString());
            return null;
        });

        openAsGlbDocument(fixtureBytes());
        openProjectSurface();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final float density = activity.getResources().getDisplayMetrics().density;
            final int floor = Math.round(48f * density);
            for (int id : new int[]{R.id.import_glb_preview, R.id.toggle_imported_preview,
                    R.id.clear_imported_preview}) {
                final View control = workspace.findViewById(id);
                final String name = activity.getResources().getResourceEntryName(id);
                assertEquals(name + " is drawn", View.VISIBLE, control.getVisibility());
                assertTrue(name + " must meet the 48 dp floor, was " + control.getHeight(),
                        control.getHeight() >= floor - 1);
            }
            return null;
        });
        closeProjectSurface();
    }

    // -----------------------------------------------------------------------
    // Evidence
    // -----------------------------------------------------------------------

    /**
     * Writes the fixture's structure and hash where the evidence script can
     * collect them. The hash is only meaningful because every byte of the
     * fixture is an integer over a power of two — no libm, no rounding mode.
     */
    @Test
    public void glbir1_evidenceFixtureFingerprint() {
        final byte[] fixture = fixtureBytes();
        assertArrayEquals("the fixture must be byte-deterministic", fixture, fixtureBytes());

        openAsGlbDocument(fixture);
        final int[] counts = previewCounts();
        final float[] bounds = previewBounds();

        final StringBuilder out = new StringBuilder();
        out.append("# GLB-IMPORT-R1 Nomad-like compatibility fixture\n");
        out.append("# Synthetic. NOT the owner's `1 lowpoly.glb`; it reproduces the\n");
        out.append("# structural feature set of that file, not its geometry.\n\n");
        out.append("bytes=").append(fixture.length).append('\n');
        out.append("sha256=").append(sha256(fixture)).append('\n');
        out.append("meshes=").append(counts[0]).append('\n');
        out.append("vertices=").append(counts[1]).append('\n');
        out.append("triangles=").append(counts[2]).append('\n');
        out.append("primitives=").append(counts[3]).append('\n');
        out.append(String.format(Locale.US,
                "world_min=%.6f,%.6f,%.6f%nworld_max=%.6f,%.6f,%.6f%n", bounds[0], bounds[1],
                bounds[2], bounds[3], bounds[4], bounds[5]));
        out.append("node_transform=matrix (column-major, non-orthogonal, non-uniform)\n");
        out.append("normals=generated (the file states none)\n");
        out.append("ignored_attributes=COLOR_0,COLOR_1,TEXCOORD_0\n");
        out.append("double_sided=true\n");
        writeReport("nomad_like_fixture.txt", out.toString());
    }

    /**
     * A screenshot of the imported fixture, from a fixed camera pose.
     *
     * <p>The distance frames the fixture's whole baked extent — roughly 3.4 m
     * across, 5.8 m tall and 4.0 m deep once the node matrix is applied — so
     * the capture shows the object rather than a corner of it. Pinned rather
     * than fitted, so the image is reproducible run to run.
     */
    @Test
    public void glbir1_evidenceImportedFixtureScreenshot() {
        openAsGlbDocument(fixtureBytes());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(NativeViewport.debugSetCameraPose(0.9f, 0.5f, 16.0f));
            return null;
        });
        settleFrames();
        capture("nomad_like_preview.png");
    }

    // -----------------------------------------------------------------------
    // An independent little reader, for the world-bounds comparison
    // -----------------------------------------------------------------------

    /**
     * The fixture's world bounds, computed here from its own bytes.
     *
     * <p>Deliberately not {@code GlbDocument}: that class is the EXPORTER's
     * conformance checker and requires exactly one primitive with a NORMAL,
     * which is precisely what an external file does not have to be. This reads
     * the one accessor and the one matrix it needs and nothing else — a third
     * implementation, so agreement between it and the product means something.
     */
    private static float[] expectedWorldBounds(byte[] glb) {
        try {
            final ByteBuffer buffer = ByteBuffer.wrap(glb).order(ByteOrder.LITTLE_ENDIAN);
            final int jsonLength = buffer.getInt(12);
            final String json = new String(glb, 20, jsonLength, StandardCharsets.UTF_8).trim();
            final int binHeader = 20 + jsonLength;
            final int binStart = binHeader + 8;

            final JSONObject root = new JSONObject(json);
            final JSONArray matrix = root.getJSONArray("nodes").getJSONObject(0)
                    .getJSONArray("matrix");
            final float[] m = new float[16];
            for (int i = 0; i < 16; ++i) {
                m[i] = (float) matrix.getDouble(i);
            }

            final JSONObject accessor = root.getJSONArray("accessors").getJSONObject(0);
            final JSONObject view = root.getJSONArray("bufferViews")
                    .getJSONObject(accessor.getInt("bufferView"));
            final int count = accessor.getInt("count");
            final int offset = binStart + view.optInt("byteOffset", 0)
                    + accessor.optInt("byteOffset", 0);

            final float[] bounds = new float[6];
            for (int v = 0; v < count; ++v) {
                final float x = buffer.getFloat(offset + v * 12);
                final float y = buffer.getFloat(offset + v * 12 + 4);
                final float z = buffer.getFloat(offset + v * 12 + 8);
                // Column-major, column-vector: world = M * local.
                final float[] world = {
                        m[0] * x + m[4] * y + m[8] * z + m[12],
                        m[1] * x + m[5] * y + m[9] * z + m[13],
                        m[2] * x + m[6] * y + m[10] * z + m[14]};
                for (int axis = 0; axis < 3; ++axis) {
                    if (v == 0) {
                        bounds[axis] = world[axis];
                        bounds[3 + axis] = world[axis];
                    } else {
                        bounds[axis] = Math.min(bounds[axis], world[axis]);
                        bounds[3 + axis] = Math.max(bounds[3 + axis], world[axis]);
                    }
                }
            }
            return bounds;
        } catch (Exception error) {
            throw new AssertionError("the fixture must be readable: " + error, error);
        }
    }

    /** The same GLB with a required extension it cannot possibly implement. */
    private static byte[] withRequiredExtension(byte[] glb) {
        final ByteBuffer buffer = ByteBuffer.wrap(glb).order(ByteOrder.LITTLE_ENDIAN);
        final int jsonLength = buffer.getInt(12);
        final String json = new String(glb, 20, jsonLength, StandardCharsets.UTF_8);
        final String edited = json.replaceFirst("\\{\"asset\":",
                "{\"extensionsRequired\":[\"KHR_draco_mesh_compression\"],\"asset\":");
        assertNotEquals("the edit must have landed", json, edited);

        final StringBuilder padded = new StringBuilder(edited.trim());
        while ((padded.length() % 4) != 0) {
            padded.append(' ');
        }
        final byte[] newJson = padded.toString().getBytes(StandardCharsets.UTF_8);
        final int binHeader = 20 + jsonLength;
        final int binLength = buffer.getInt(binHeader);

        final ByteBuffer out = ByteBuffer
                .allocate(12 + 8 + newJson.length + 8 + binLength)
                .order(ByteOrder.LITTLE_ENDIAN);
        out.putInt(0x46546C67);
        out.putInt(2);
        out.putInt(12 + 8 + newJson.length + 8 + binLength);
        out.putInt(newJson.length);
        out.putInt(0x4E4F534A);
        out.put(newJson);
        out.putInt(binLength);
        out.putInt(0x004E4942);
        out.put(glb, binHeader + 8, binLength);
        return out.array();
    }

    // -----------------------------------------------------------------------
    // Shared machinery
    // -----------------------------------------------------------------------

    private byte[] fixtureBytes() {
        final byte[] bytes = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.nomadLikeGlbFixture());
        assertNotNull("the fixture must build", bytes);
        assertTrue("and it must be a real file", bytes.length > 10000);
        return bytes;
    }

    /**
     * Drives the REAL picker-result seam: bytes on the device, a {@code Uri}
     * the system would have handed back, and the production handler that turns
     * one into the other. Only the system's own document UI is skipped, because
     * it is another app's surface and cannot be driven reliably.
     */
    private void openAsGlbDocument(byte[] bytes) {
        final File file = new File(scratch, "external-" + bytes.length + ".glb");
        writeFile(file, bytes);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onOpenGlbDocumentChosen(Uri.fromFile(file));
            return null;
        });
        settleLayout();
    }

    private int importPreview(final byte[] bytes) {
        final int status = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.importGlbPreview(bytes));
        settleLayout();
        return status;
    }

    private String statusToken(final int status) {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.glbImportStatusToken(status));
    }

    private int statusCategory(final int status) {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.glbImportStatusCategory(status));
    }

    private void setPreviewVisible(final boolean visible) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.setGlbPreviewVisible(visible);
            workspace.syncFromNative();
            return null;
        });
        settleLayout();
    }

    private void clearPreview() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.clearGlbPreview();
            return null;
        });
    }

    private boolean loaded() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.glbPreviewLoaded());
    }

    private boolean previewVisible() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.glbPreviewVisible());
    }

    private int[] previewCounts() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int[] counts = new int[4];
            NativeViewport.glbPreviewCounts(counts);
            return counts;
        });
    }

    private float[] previewBounds() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final float[] bounds = new float[6];
            assertTrue("a loaded preview must have bounds",
                    NativeViewport.glbPreviewBounds(bounds));
            return bounds;
        });
    }


    private byte[] encodeProject() {
        final byte[] bytes = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.encodeProject());
        assertNotNull(bytes);
        return bytes;
    }

    private byte[] exportGlb() {
        final byte[] bytes = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.exportGlb());
        assertNotNull("the project must export", bytes);
        return bytes;
    }

    private long fingerprint() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.projectFingerprint());
    }

    private long activeBodyId() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneActiveBodyId());
    }

    private long meshRevision() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.constructionMeshRevision());
    }

    private int undoDepth() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.constructionUndoDepth());
    }

    private int redoDepth() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.constructionRedoDepth());
    }

    private long[] sceneBodyIds() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final long[] ids = new long[NativeViewport.sceneBodyCount()];
            final int written = NativeViewport.sceneBodyIds(ids);
            return Arrays.copyOf(ids, written);
        });
    }

    private void openForgeSample(String assetName) {
        final File document = new File(scratch, assetName.replace('/', '_'));
        writeFile(document, readAsset(assetName));
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onOpenProjectDocumentChosen(Uri.fromFile(document));
            return null;
        });
        settleLayout();
    }

    private void openProjectSurface() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            if (!workspace.projectPopover().isOpen()) {
                workspace.findViewById(R.id.project_actions_button).performClick();
            }
            return null;
        });
        settleLayout();
    }

    private void closeProjectSurface() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            if (workspace.projectPopover().isOpen()) {
                workspace.findViewById(R.id.project_actions_button).performClick();
            }
            return null;
        });
        settleLayout();
    }

    private void settleFrames() {
        for (int i = 0; i < 6; ++i) {
            settleLayout();
        }
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private void capture(String name) {
        final android.graphics.Bitmap shot =
                InstrumentationRegistry.getInstrumentation().getUiAutomation().takeScreenshot();
        assertNotNull("the harness must be able to capture the screen", shot);
        final File directory = context().getExternalFilesDir(null);
        assertNotNull(directory);
        try (FileOutputStream out = new FileOutputStream(new File(directory, name))) {
            shot.compress(android.graphics.Bitmap.CompressFormat.PNG, 100, out);
        } catch (IOException error) {
            throw new AssertionError(error);
        } finally {
            shot.recycle();
        }
    }

    private void writeReport(String name, String text) {
        final File directory = context().getExternalFilesDir(null);
        assertNotNull("this device must expose an external files directory", directory);
        writeFile(new File(directory, name), text.getBytes(StandardCharsets.UTF_8));
    }

    private static String sha256(byte[] bytes) {
        try {
            final byte[] digest = MessageDigest.getInstance("SHA-256").digest(bytes);
            final StringBuilder out = new StringBuilder(digest.length * 2);
            for (byte value : digest) {
                out.append(String.format(Locale.US, "%02x", value));
            }
            return out.toString();
        } catch (Exception error) {
            throw new AssertionError(error);
        }
    }

    private static byte[] readAsset(String name) {
        try (InputStream in = InstrumentationRegistry.getInstrumentation().getContext()
                .getAssets().open(name)) {
            final ByteArrayOutputStream out = new ByteArrayOutputStream();
            final byte[] chunk = new byte[8192];
            int read;
            while ((read = in.read(chunk)) > 0) {
                out.write(chunk, 0, read);
            }
            return out.toByteArray();
        } catch (IOException error) {
            throw new AssertionError("missing test asset " + name, error);
        }
    }

    private static void writeFile(File file, byte[] bytes) {
        try (FileOutputStream out = new FileOutputStream(file)) {
            out.write(bytes);
        } catch (IOException error) {
            throw new AssertionError(error);
        }
    }

    private static void deleteRecursively(File file) {
        final File[] children = file.listFiles();
        if (children != null) {
            for (File child : children) {
                deleteRecursively(child);
            }
        }
        file.delete();
    }
}
