package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;

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
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * `FSR1C-13..16` and `E2ER1C-01..06`: the early GLB export, read back by
 * something that is not the exporter.
 *
 * <h2>What makes these cases worth anything</h2>
 *
 * <p>Every assertion here goes through {@link GlbDocument}, a reader written
 * from the glTF 2.0 specification that shares no code with the writer. If the
 * exporter and the reader agree on a wrong offset, a wrong chunk boundary or a
 * wrong matrix order, they have to have made the same mistake twice, from two
 * different documents, which is the closest an in-repo test can get to "another
 * program opened it".
 *
 * <p>The two `.forge` documents these cases load are the permanent golden
 * corpus fixtures, byte-identical to {@code testdata/forge/v1/} and produced by
 * the independent PowerShell encoder — so the INPUT to the export is pinned by
 * a digest as firmly as the output is checked by an independent reader.
 *
 * <h2>What is deliberately not claimed</h2>
 *
 * <p>These cases do not run Blender, Godot or any external importer. They prove
 * the file is a conformant glTF 2.0 binary carrying the geometry ForgeShape
 * says it carries. Whether a particular third-party program likes it is a
 * separate, owner-run check.
 */
@RunWith(AndroidJUnit4.class)
public final class GlbExportTest {

    /** The corpus fixture holding six placed Construction bodies. */
    private static final String SAMPLE_CONSTRUCTION = "forge/construction_multibody_v1.forge";

    /** The corpus fixture holding a Construction companion and a sculpted body. */
    private static final String SAMPLE_SCULPT = "forge/sculpt_mixed_v1.forge";

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private final File scratch = new File(context().getCacheDir(), "glb-export-test");

    private static Context context() {
        return InstrumentationRegistry.getInstrumentation().getTargetContext();
    }

    @Before
    public void setUp() {
        context().deleteFile(ProjectSlot.SLOT_FILE_NAME);
        context().deleteFile(ProjectCheckpoint.CHECKPOINT_FILE_NAME);
        deleteRecursively(scratch);
        assertTrue("the scratch directory must exist", scratch.mkdirs());
        resetToBaselineConstruction(rule.getScenario());
    }

    @After
    public void tearDown() {
        deleteRecursively(scratch);
        context().deleteFile(ProjectSlot.SLOT_FILE_NAME);
        context().deleteFile(ProjectCheckpoint.CHECKPOINT_FILE_NAME);
        resetToBaselineConstruction(rule.getScenario());
    }

    // -----------------------------------------------------------------------
    // FSR1C-13: the bytes are a glTF 2.0 binary, as a foreign reader sees one
    // -----------------------------------------------------------------------

    @Test
    public void fsr1c13_theExportIsAWellFormedSingleFileGlb() {
        applyBox(1.0, 2.0, 4.0);
        final byte[] bytes = export();
        final GlbDocument document = GlbDocument.parse(bytes);

        assertEquals("no structural problem: " + document.problemSummary(),
                0, document.validate().size());
        assertEquals("container version", 2, document.version());
        assertEquals("the header's length is the file's length",
                document.actualLength(), document.declaredLength());
        assertEquals("a GLB is 4-byte framed throughout", 0, bytes.length % 4);
        assertTrue("the BIN chunk carries the geometry", document.bin().length > 0);
        assertEquals("one file, no sidecar: nothing may name an external resource",
                -1, document.json().indexOf("\"uri\""));
        assertTrue("and it says who wrote it",
                document.json().contains("\"generator\":\"ForgeShape\""));
    }

    @Test
    public void fsr1c13_aTruncatedExportIsRejectedByTheIndependentReader() {
        // The reader is only evidence if it can fail. Cutting the file short
        // must produce problems rather than a quietly smaller model — otherwise
        // every other case here proves nothing.
        final byte[] bytes = export();
        final byte[] truncated = Arrays.copyOf(bytes, bytes.length - 64);
        boolean rejected;
        try {
            rejected = !GlbDocument.parse(truncated).validate().isEmpty();
        } catch (GlbDocument.MalformedGlb expected) {
            rejected = true;
        }
        assertTrue("a truncated GLB must not read as valid", rejected);
    }

    // -----------------------------------------------------------------------
    // FSR1C-14: one ForgeShape metre is one glTF metre, and +Y is up
    // -----------------------------------------------------------------------

    @Test
    public void fsr1c14_metresSurviveUnscaledAndTheUpAxisIsY() {
        applyBox(1.0, 2.0, 4.0);
        final GlbDocument.Body body = activeBody(export());
        final float[] bounds = body.localBounds();

        assertEquals("width stays on X, in metres", 1.0f, bounds[3] - bounds[0], 1e-5f);
        assertEquals("HEIGHT stays on Y — this is the up-axis assertion",
                2.0f, bounds[4] - bounds[1], 1e-5f);
        assertEquals("depth stays on Z, in metres", 4.0f, bounds[5] - bounds[2], 1e-5f);
        // A box is centred on its own origin, so any global conversion scale
        // would show as an off-centre extent as well as a wrong size.
        assertEquals(-0.5f, bounds[0], 1e-5f);
        assertEquals(-1.0f, bounds[1], 1e-5f);
        assertEquals(-2.0f, bounds[2], 1e-5f);
    }

    @Test
    public void fsr1c14_noConversionNodeIsInsertedAboveTheBody() {
        applyBox(1.0, 2.0, 4.0);
        final byte[] bytes = export();
        final GlbDocument document = GlbDocument.parse(bytes);
        assertTrue("the scene reached the file", document.bodies().size() >= 1);
        assertEquals("exactly one matrix per body and not one more — a"
                        + " Y-up-to-Z-up fixer node would be the extra one",
                document.bodies().size(), countOccurrences(document.json(), "\"matrix\""));
    }

    // -----------------------------------------------------------------------
    // FSR1C-15: the node carries the placement; the vertices never do
    // -----------------------------------------------------------------------

    @Test
    public void fsr1c15_placementLivesInTheNodeMatrixAndNotInTheVertices() {
        applyBox(1.0, 2.0, 4.0);
        final float[] restingVertices = activeBody(export()).positions;

        applyPlacement(3.5, -1.25, 0.75, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
        final GlbDocument.Body moved = activeBody(export());

        assertArrayEquals("moving a body must not move one vertex",
                restingVertices, moved.positions, 0.0f);
        assertEquals("translation is the matrix's LAST COLUMN, element 12",
                3.5f, moved.matrix[12], 1e-5f);
        assertEquals(-1.25f, moved.matrix[13], 1e-5f);
        assertEquals(0.75f, moved.matrix[14], 1e-5f);
        assertArrayEquals("and the bottom row of a column-major matrix is 0,0,0,1",
                new float[]{0f, 0f, 0f, 1f},
                new float[]{moved.matrix[3], moved.matrix[7], moved.matrix[11],
                        moved.matrix[15]}, 1e-6f);
        // Restated through the reader's own matrix arithmetic: with no
        // rotation and unit scale, every world vertex is its local vertex plus
        // the translation. If the reader multiplied a transposed matrix this
        // would not hold, so the two claims check each other.
        final float[] local = moved.localVertex(0);
        final float[] world = moved.worldVertex(0);
        assertArrayEquals(new float[]{local[0] + 3.5f, local[1] - 1.25f, local[2] + 0.75f},
                world, 1e-4f);
    }

    @Test
    public void fsr1c15_scaleIsInTheMatrixAndNotBakedIntoTheGeometry() {
        applyBox(1.0, 2.0, 4.0);
        applyPlacement(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 2.0, 3.0, 0.5);
        final GlbDocument.Body body = activeBody(export());
        final float[] bounds = body.localBounds();

        assertEquals("the geometry is still the 1 m dimension the user typed",
                1.0f, bounds[3] - bounds[0], 1e-5f);
        assertEquals("a column's length is that axis's scale — X",
                2.0f, columnLength(body.matrix, 0), 1e-5f);
        assertEquals("Y", 3.0f, columnLength(body.matrix, 1), 1e-5f);
        assertEquals("Z", 0.5f, columnLength(body.matrix, 2), 1e-5f);
    }

    // -----------------------------------------------------------------------
    // FSR1C-16: every body, once, with usable surfaces
    // -----------------------------------------------------------------------

    @Test
    public void fsr1c16_everySceneBodyIsExportedOnceAndNamedByItsObjectId() {
        final long second = addBody();
        final long[] ids = sceneBodyIds();
        assertTrue("this case needs more than one body", ids.length >= 2);
        assertTrue(contains(ids, second));

        final GlbDocument document = GlbDocument.parse(export());
        assertEquals("no structural problem: " + document.problemSummary(),
                0, document.validate().size());
        assertEquals("one node per body, no more and no fewer",
                ids.length, document.bodies().size());
        for (int i = 0; i < ids.length; ++i) {
            assertEquals("scene order is preserved and the name is the ObjectId",
                    "Body_" + ids[i], document.bodies().get(i).name);
        }
    }

    @Test
    public void fsr1c16_normalsAreUnitLengthAndTrianglesWindOutward() {
        applyBox(1.0, 2.0, 4.0);
        final GlbDocument.Body body = activeBody(export());
        assertTrue("a box exports real triangles", body.triangleCount() >= 12);

        for (int i = 0; i < body.vertexCount(); ++i) {
            final float x = body.normals[i * 3];
            final float y = body.normals[i * 3 + 1];
            final float z = body.normals[i * 3 + 2];
            assertEquals("normal " + i + " must be unit length",
                    1.0f, (float) Math.sqrt(x * x + y * y + z * z), 1e-3f);
        }
        assertOutwardWinding(body);
    }

    // -----------------------------------------------------------------------
    // E2ER1C-01/02: a whole saved project, six placed bodies, on the device
    // -----------------------------------------------------------------------

    @Test
    public void e2er1c01_aSixBodyProjectExportsAsOneValidGlbWithSixDistinctNodes() {
        openSample(SAMPLE_CONSTRUCTION);
        final GlbDocument document = GlbDocument.parse(export());

        assertEquals("no structural problem: " + document.problemSummary(),
                0, document.validate().size());
        final long[] ids = sceneBodyIds();
        assertEquals("the fixture's six bodies", 6, ids.length);
        assertEquals("and six nodes", 6, document.bodies().size());

        final List<String> matrices = new ArrayList<>();
        for (GlbDocument.Body body : document.bodies()) {
            final String key = Arrays.toString(body.matrix);
            assertFalse("six differently placed bodies must not share a matrix: " + key,
                    matrices.contains(key));
            matrices.add(key);
            assertTrue("every body carries geometry", body.triangleCount() > 0);
        }
    }

    @Test
    public void e2er1c02_theFixturesTranslationsRotationsAndScalesReachTheFile() {
        openSample(SAMPLE_CONSTRUCTION);
        final GlbDocument document = GlbDocument.parse(export());
        final long[] ids = sceneBodyIds();

        // The fixture places body i at (0.5i, -0.25i, 1.25i) with scale
        // (1 + 0.25i, 2, 0.5) and rotation (370, -45.5, 12.25i) — every value
        // asymmetric on purpose, so no axis can stand in for another.
        for (int i = 0; i < ids.length; ++i) {
            final int n = (int) ids[i];
            final GlbDocument.Body body = document.bodies().get(i);
            final String who = "body " + n;
            assertEquals(who + " X", 0.5f * n, body.matrix[12], 1e-5f);
            assertEquals(who + " Y", -0.25f * n, body.matrix[13], 1e-5f);
            assertEquals(who + " Z", 1.25f * n, body.matrix[14], 1e-5f);
            assertEquals(who + " scale X", 1.0f + 0.25f * n,
                    columnLength(body.matrix, 0), 1e-4f);
            assertEquals(who + " scale Y", 2.0f, columnLength(body.matrix, 1), 1e-4f);
            assertEquals(who + " scale Z", 0.5f, columnLength(body.matrix, 2), 1e-4f);
            assertTrue(who + " is rotated, so no column may stay axis-aligned",
                    Math.abs(body.matrix[0]) < columnLength(body.matrix, 0) - 1e-3f);
        }
    }

    // -----------------------------------------------------------------------
    // E2ER1C-03: a sculpted body exports its sculpt mesh, not its source shape
    // -----------------------------------------------------------------------

    @Test
    public void e2er1c03_aSculptProjectExportsTheSculptMeshForTheBodyThatHasOne() {
        openSample(SAMPLE_SCULPT);
        assertEquals("the fixture reopens in Sculpt", NativeViewport.MODE_SCULPT,
                onWorkspace(rule.getScenario(),
                        (activity, workspace) -> NativeViewport.productMode()).intValue());

        final GlbDocument document = GlbDocument.parse(export());
        assertEquals("no structural problem: " + document.problemSummary(),
                0, document.validate().size());
        assertEquals("both bodies are exported", 2, document.bodies().size());

        // The sculpted body is the fixture's tetrahedron, whose corners are
        // deliberately NOT the 1.5 m sphere its Construction Source describes.
        final GlbDocument.Body sculpted = document.bodies().get(1);
        final float[] bounds = sculpted.localBounds();
        assertEquals("the tetrahedron's own X extent", 0.0f, bounds[0], 1e-5f);
        assertEquals(1.5f, bounds[3], 1e-5f);
        assertEquals(0.0f, bounds[1], 1e-5f);
        assertEquals(1.25f, bounds[4], 1e-5f);
        assertEquals(0.0f, bounds[2], 1e-5f);
        assertEquals(1.75f, bounds[5], 1e-5f);
        assertEquals("a closed tetrahedron is four triangles", 4, sculpted.triangleCount());

        // ...and the companion body, which was never sculpted, exports the
        // Construction shape it still is: the fixture's 2 x 1 x 0.5 m box.
        final float[] companion = document.bodies().get(0).localBounds();
        assertEquals("companion width", 2.0f, companion[3] - companion[0], 1e-5f);
        assertEquals("companion height", 1.0f, companion[4] - companion[1], 1e-5f);
        assertEquals("companion depth", 0.5f, companion[5] - companion[2], 1e-5f);
    }

    // -----------------------------------------------------------------------
    // E2ER1C-04/05: the real Android write path, and the real cancel
    // -----------------------------------------------------------------------

    @Test
    public void e2er1c04_exportWritesTheFileTheUserChoseAndItParses() {
        openSample(SAMPLE_CONSTRUCTION);
        final byte[] expected = export();
        final File destination = new File(scratch, "model.glb");

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onExportGlbRequestedForTest(expected);
            workspace.onCreateGlbDocumentChosen(Uri.fromFile(destination));
            return null;
        });
        settleLayout();

        assertTrue("the file exists where the user chose", destination.isFile());
        final byte[] written = readFile(destination);
        assertArrayEquals("byte for byte what the exporter produced", expected, written);
        assertEquals("and what is on disk is a valid GLB", 0,
                GlbDocument.parse(written).validate().size());
    }

    @Test
    public void e2er1c04_theCreateIntentAsksForAGlbByNameAndType() {
        final Intent intent = ProjectTransfer.createGlbDocumentIntent();
        assertEquals(Intent.ACTION_CREATE_DOCUMENT, intent.getAction());
        assertTrue(intent.hasCategory(Intent.CATEGORY_OPENABLE));
        assertEquals("model/gltf-binary", intent.getType());
        final String title = intent.getStringExtra(Intent.EXTRA_TITLE);
        assertNotNull(title);
        assertTrue("the suggested name must end .glb: " + title, title.endsWith(".glb"));
    }

    @Test
    public void e2er1c05_cancellingWritesNothingAndChangesNothing() {
        openSample(SAMPLE_CONSTRUCTION);
        final long fingerprint = fingerprint();
        final byte[] bytes = export();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onExportGlbRequestedForTest(bytes);
            workspace.onCreateGlbDocumentChosen(null);
            return null;
        });
        settleLayout();

        assertEquals("a cancelled export leaves the project exactly as it was",
                fingerprint, fingerprint());
        assertEquals("and writes no file at all", 0, scratch.listFiles().length);

        // The staged bytes must not survive a cancel either: a later export to a
        // different destination has to encode the project as it is then.
        final File destination = new File(scratch, "after-cancel.glb");
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onCreateGlbDocumentChosen(Uri.fromFile(destination));
            return null;
        });
        settleLayout();
        assertFalse("a second answer with nothing staged writes nothing",
                destination.exists());
    }

    // -----------------------------------------------------------------------
    // E2ER1C-06: export is a read
    // -----------------------------------------------------------------------

    @Test
    public void e2er1c06_exportingMutatesNoProjectStateAndTouchesNoForgeSlot() {
        openSample(SAMPLE_CONSTRUCTION);
        final double[] before = WorkspaceTestSupport.nativeSnapshot();
        final long fingerprintBefore = fingerprint();
        final long revisionBefore = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.constructionMeshRevision());
        final int undoBefore = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.constructionUndoDepth());
        final int redoBefore = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.constructionRedoDepth());
        final long activeBefore = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneActiveBodyId());
        final long[] idsBefore = sceneBodyIds();

        final File destination = new File(scratch, "read-only.glb");
        final byte[] bytes = export();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onExportGlbRequestedForTest(bytes);
            workspace.onCreateGlbDocumentChosen(Uri.fromFile(destination));
            return null;
        });
        settleLayout();

        final String drift = WorkspaceTestSupport.describeSnapshotDifference(
                before, WorkspaceTestSupport.nativeSnapshot());
        assertEquals("the whole observable native state is unchanged:" + drift, "", drift);
        assertEquals("no revision was minted", revisionBefore,
                (long) onWorkspace(rule.getScenario(),
                        (activity, workspace) -> NativeViewport.constructionMeshRevision()));
        assertEquals("history depth is untouched", undoBefore,
                (int) onWorkspace(rule.getScenario(),
                        (activity, workspace) -> NativeViewport.constructionUndoDepth()));
        assertEquals("including redo", redoBefore,
                (int) onWorkspace(rule.getScenario(),
                        (activity, workspace) -> NativeViewport.constructionRedoDepth()));
        assertEquals("no ObjectId moved", activeBefore,
                (long) onWorkspace(rule.getScenario(),
                        (activity, workspace) -> NativeViewport.sceneActiveBodyId()));
        assertArrayEquals("and no body appeared or vanished", idsBefore, sceneBodyIds());
        assertEquals("the project itself is identical", fingerprintBefore, fingerprint());

        // The `.forge` slots are the other thing an export must never write.
        assertFalse("export must not create the manual slot",
                context().getFileStreamPath(ProjectSlot.SLOT_FILE_NAME).exists());
        assertFalse("nor the recovery checkpoint",
                context().getFileStreamPath(ProjectCheckpoint.CHECKPOINT_FILE_NAME).exists());
    }

    // -----------------------------------------------------------------------
    // Evidence: the two sentinel files the owner-facing bundle carries
    // -----------------------------------------------------------------------
    //
    // These are ordinary cases — they load a fixture, export it and validate
    // the result like every case above — that additionally LEAVE the file where
    // scripts\run-glb-export-evidence.ps1 can pull it. The external files
    // directory, not the cache, because the cache is wiped between cases and
    // the point of these two is that the artifact outlives the run.

    @Test
    public void e2er1c_evidenceConstructionSentinel() {
        openSample(SAMPLE_CONSTRUCTION);
        writeSentinel("construction_sentinel.glb", export());
    }

    @Test
    public void e2er1c_evidenceSculptSentinel() {
        openSample(SAMPLE_SCULPT);
        writeSentinel("sculpt_sentinel.glb", export());
    }

    private void writeSentinel(String name, byte[] bytes) {
        final GlbDocument document = GlbDocument.parse(bytes);
        assertEquals("a sentinel is only evidence if it is valid: "
                + document.problemSummary(), 0, document.validate().size());
        final File directory = context().getExternalFilesDir(null);
        assertNotNull("this device must expose an external files directory", directory);
        final File file = new File(directory, name);
        writeFile(file, bytes);
        assertEquals(bytes.length, (int) file.length());
    }

    // -----------------------------------------------------------------------
    // Shared machinery
    // -----------------------------------------------------------------------

    private byte[] export() {
        final byte[] bytes = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.exportGlb());
        assertNotNull("the exporter produced nothing", bytes);
        assertTrue(bytes.length > 12);
        return bytes;
    }

    /**
     * The exported node for the body the Construction editors are acting on.
     *
     * <p>Found BY NAME rather than by position, because these cases share a
     * process with cases that load a six-body fixture and the scene they start
     * from is not theirs to assume. The name is the ObjectId, which is the
     * stable identity; the index in the file is not.
     */
    private GlbDocument.Body activeBody(byte[] bytes) {
        final GlbDocument document = GlbDocument.parse(bytes);
        assertEquals("no structural problem: " + document.problemSummary(),
                0, document.validate().size());
        final String wanted = "Body_" + onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneActiveBodyId());
        for (GlbDocument.Body body : document.bodies()) {
            if (wanted.equals(body.name)) {
                return body;
            }
        }
        throw new AssertionError("the active body " + wanted + " is not in the export");
    }

    private void applyBox(final double w, final double h, final double d) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.APPLY_APPLIED,
                    NativeViewport.applyConstructionBox(w, h, d));
            return null;
        });
        settleLayout();
    }

    private void applyPlacement(final double px, final double py, final double pz,
                                final double rx, final double ry, final double rz,
                                final double sx, final double sy, final double sz) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.APPLY_APPLIED,
                    NativeViewport.applyBoxTransform(px, py, pz, rx, ry, rz, sx, sy, sz));
            return null;
        });
        settleLayout();
    }

    private long addBody() {
        final long id = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneAddBody());
        settleLayout();
        assertTrue("a second body was added", id != NativeViewport.NO_OBJECT);
        return id;
    }

    private long[] sceneBodyIds() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final long[] ids = new long[NativeViewport.sceneBodyCount()];
            final int written = NativeViewport.sceneBodyIds(ids);
            return Arrays.copyOf(ids, written);
        });
    }

    private long fingerprint() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.projectFingerprint());
    }

    /** Loads a golden-corpus fixture through the production Open File handler. */
    private void openSample(String assetName) {
        final File document = new File(scratch, assetName.replace('/', '_'));
        writeFile(document, readAsset(assetName));
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onOpenProjectDocumentChosen(Uri.fromFile(document));
            return null;
        });
        settleLayout();
        assertTrue("the fixture must have loaded", document.delete());
    }

    /**
     * Asserts that every triangle faces away from the body's own origin.
     *
     * <p>Only sound for a shape that encloses its origin, which is why this is
     * used on a box. Counter-clockwise from OUTSIDE is glTF's rule and
     * ForgeShape's; a file that got it backwards renders inside-out in every
     * consumer that culls, and looks correct in every one that does not — so it
     * has to be checked here rather than seen.
     */
    private static void assertOutwardWinding(GlbDocument.Body body) {
        for (int t = 0; t < body.triangleCount(); ++t) {
            final float[] a = body.localVertex(body.indices[t * 3]);
            final float[] b = body.localVertex(body.indices[t * 3 + 1]);
            final float[] c = body.localVertex(body.indices[t * 3 + 2]);
            final float[] cross = {
                    (b[1] - a[1]) * (c[2] - a[2]) - (b[2] - a[2]) * (c[1] - a[1]),
                    (b[2] - a[2]) * (c[0] - a[0]) - (b[0] - a[0]) * (c[2] - a[2]),
                    (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])};
            final float dot = cross[0] * (a[0] + b[0] + c[0]) / 3f
                    + cross[1] * (a[1] + b[1] + c[1]) / 3f
                    + cross[2] * (a[2] + b[2] + c[2]) / 3f;
            assertTrue("triangle " + t + " winds inward", dot > 0f);
        }
    }

    private static float columnLength(float[] matrix, int column) {
        final float x = matrix[column * 4];
        final float y = matrix[column * 4 + 1];
        final float z = matrix[column * 4 + 2];
        return (float) Math.sqrt(x * x + y * y + z * z);
    }

    private static int countOccurrences(String haystack, String needle) {
        int count = 0;
        for (int at = haystack.indexOf(needle); at >= 0;
                at = haystack.indexOf(needle, at + needle.length())) {
            count++;
        }
        return count;
    }

    private static boolean contains(long[] values, long wanted) {
        for (long value : values) {
            if (value == wanted) {
                return true;
            }
        }
        return false;
    }

    private static byte[] readAsset(String name) {
        try (InputStream in = InstrumentationRegistry.getInstrumentation()
                .getContext().getAssets().open(name)) {
            final ByteArrayOutputStream out = new ByteArrayOutputStream();
            final byte[] chunk = new byte[8192];
            int read;
            while ((read = in.read(chunk)) > 0) {
                out.write(chunk, 0, read);
            }
            return out.toByteArray();
        } catch (IOException error) {
            throw new AssertionError("the fixture " + name + " must ship with the tests", error);
        }
    }

    private static void writeFile(File file, byte[] bytes) {
        try (FileOutputStream out = new FileOutputStream(file)) {
            out.write(bytes);
        } catch (IOException error) {
            throw new AssertionError(error);
        }
    }

    private static byte[] readFile(File file) {
        final byte[] bytes = new byte[(int) file.length()];
        try (java.io.FileInputStream in = new java.io.FileInputStream(file)) {
            int read = 0;
            while (read < bytes.length) {
                final int step = in.read(bytes, read, bytes.length - read);
                if (step <= 0) {
                    break;
                }
                read += step;
            }
        } catch (IOException error) {
            throw new AssertionError(error);
        }
        return bytes;
    }

    private static void deleteRecursively(File file) {
        if (file == null || !file.exists()) {
            return;
        }
        final File[] children = file.listFiles();
        if (children != null) {
            for (File child : children) {
                deleteRecursively(child);
            }
        }
        file.delete();
    }
}
