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
        assertEquals("no node carries a matrix at all under ARCH-OWNER-07,"
                        + " so a Y-up-to-Z-up fixer node would be the only one",
                0, countOccurrences(document.json(), "\"matrix\""));
        assertEquals("and exactly one translation per body, never one more",
                document.bodies().size(),
                countOccurrences(document.json(), "\"translation\""));
    }

    // -----------------------------------------------------------------------
    // FSR1C-15 / FSR1C-C1-01..03: the node carries T, and only T
    // -----------------------------------------------------------------------

    @Test
    public void fsr1cC1_01to03_theNodeCarriesTranslationOnlyWithIdentityRotationAndScale() {
        applyBox(1.0, 2.0, 4.0);
        applyPlacement(3.5, -1.25, 0.75, 47.5, -22.0, 13.25, 2.0, 3.0, 0.5);
        final GlbDocument.Body body = activeBody(export());

        // FSR1C-C1-03: the translation is the ForgeShape placement, unchanged
        // and unscaled — 3.5 * 2.0 would read 7.0.
        assertArrayEquals("the node translation is the authored position in metres",
                new float[]{3.5f, -1.25f, 0.75f}, body.translation, 1e-5f);
        assertTrue("and it is stated as a translation", body.hasTranslation);

        // FSR1C-C1-01 and -02: rotation and scale are ABSENT, so every reader
        // sees identity for both. Asserted on the parsed node and again on the
        // raw JSON, because "absent" is exactly the kind of claim a convenience
        // default can hide.
        assertFalse("no node rotation", body.hasRotation);
        assertFalse("no node scale", body.hasScale);
        assertFalse("and no node matrix", body.hasMatrix);
        final String json = GlbDocument.parse(export()).json();
        assertEquals(0, countOccurrences(json, "\"rotation\""));
        assertEquals(0, countOccurrences(json, "\"scale\""));
        assertEquals(0, countOccurrences(json, "\"matrix\""));
    }

    @Test
    public void fsr1cC1_04_rotationAndScaleAreBakedIntoTheGeometry() {
        applyBox(1.0, 2.0, 4.0);
        applyPlacement(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 2.0, 3.0, 0.5);
        final float[] scaled = activeBody(export()).localBounds();

        // The 1 x 2 x 4 m box arrives already 2 x 6 x 2 m, because the scale is
        // in the vertices now. Under the old policy this read 1 x 2 x 4 with
        // the scale on the node.
        assertEquals("X is baked: 1 m x 2", 2.0f, scaled[3] - scaled[0], 1e-4f);
        assertEquals("Y is baked: 2 m x 3", 6.0f, scaled[4] - scaled[1], 1e-4f);
        assertEquals("Z is baked: 4 m x 0.5", 2.0f, scaled[5] - scaled[2], 1e-4f);

        // A quarter turn about Y swaps which world axis the X and Z extents
        // land on — visible only because it is baked.
        applyPlacement(0.0, 0.0, 0.0, 0.0, 90.0, 0.0, 1.0, 1.0, 1.0);
        final float[] turned = activeBody(export()).localBounds();
        assertEquals("the 4 m depth is now the X extent", 4.0f, turned[3] - turned[0], 1e-4f);
        assertEquals("height is untouched by a Y rotation", 2.0f, turned[4] - turned[1], 1e-4f);
        assertEquals("and the 1 m width is now the Z extent", 1.0f, turned[5] - turned[2], 1e-4f);
    }

    @Test
    public void fsr1cC1_05_bakingHappensAboutTheLocalOriginAndNothingIsRecentred() {
        // A CONE, and the choice matters. A box, a plane and a sphere are all
        // centrally symmetric about their local origin, and rotating a
        // centrally symmetric point set leaves its bounding box symmetric too —
        // so a recentre-on-bounds would be invisible in any of them. A cone has
        // its apex at +Y and its base disc at -Y, so once it is turned its
        // bounds are genuinely lopsided about the origin.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.APPLY_APPLIED,
                    NativeViewport.applyConstructionCone(2.0, 3.0));
            return null;
        });
        settleLayout();
        applyPlacement(4.0, -2.0, 0.5, 25.0, -40.0, 65.0, 1.5, 1.0, 0.25);

        final GlbDocument.Body body = activeBody(export());
        final float[] bounds = body.localBounds();
        // Recentring on bounds forces min == -max on EVERY axis, so one axis
        // where it does not is the falsifying observation.
        boolean lopsided = false;
        for (int axis = 0; axis < 3; ++axis) {
            if (Math.abs(bounds[axis] + bounds[3 + axis]) > 1e-4f) {
                lopsided = true;
            }
        }
        assertTrue("the baked geometry must not be recentred on its own bounds", lopsided);
        // The pivot is still the local origin, and the node still positions it.
        assertArrayEquals("the pivot did not move into the geometry",
                new float[]{4.0f, -2.0f, 0.5f}, body.translation, 1e-5f);
    }

    /**
     * `FSR1C-C1-06`. Normals ride {@code transpose(inverse(L))}, not {@code L}.
     *
     * <p>Done by exporting the SAME body twice — once unscaled, once at 4:1:1 —
     * and carrying the first export's normals by each candidate matrix here, in
     * the test, with no formula borrowed from the product. The tessellation
     * does not depend on the transform, so the two exports have the same
     * vertices in the same order and the comparison is exact.
     *
     * <p>A SPHERE, and the choice matters. A box's face normals all lie along
     * its local axes, and for a normal parallel to a scale axis the two
     * matrices give the same DIRECTION and differ only in length — which
     * normalising then hides, so a box cannot tell them apart at all.
     */
    @Test
    public void fsr1cC1_06_normalsRideTheInverseTransposeUnderNonUniformScale() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.APPLY_APPLIED, NativeViewport.applyConstructionSphere(1.0));
            return null;
        });
        settleLayout();
        applyPlacement(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
        final GlbDocument.Body source = activeBody(export());

        applyPlacement(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 4.0, 1.0, 1.0);
        final GlbDocument.Body scaled = activeBody(export());
        assertEquals("the scale must not change the tessellation",
                source.vertexCount(), scaled.vertexCount());

        // With no rotation, L is diag(4,1,1) and transpose(inverse(L)) is
        // diag(1/4,1,1). Both written out rather than derived, so this test
        // shares no arithmetic with the exporter.
        final float[] l = {4f, 1f, 1f};
        final float[] inverseTranspose = {0.25f, 1f, 1f};
        int disagreements = 0;
        for (int i = 0; i < scaled.vertexCount(); ++i) {
            final float x = scaled.normals[i * 3];
            final float y = scaled.normals[i * 3 + 1];
            final float z = scaled.normals[i * 3 + 2];
            assertEquals("normal " + i + " must be unit length",
                    1.0f, (float) Math.sqrt(x * x + y * y + z * z), 1e-3f);

            final float[] n = {source.normals[i * 3], source.normals[i * 3 + 1],
                    source.normals[i * 3 + 2]};
            final float[] correct = normalized(n[0] * inverseTranspose[0],
                    n[1] * inverseTranspose[1], n[2] * inverseTranspose[2]);
            final float[] wrong = normalized(n[0] * l[0], n[1] * l[1], n[2] * l[2]);
            if (correct == null || wrong == null) {
                continue;
            }
            assertArrayEquals("normal " + i + " was not carried by the inverse transpose",
                    correct, new float[]{x, y, z}, 2e-3f);
            if (Math.abs(correct[0] - wrong[0]) > 1e-2f) {
                disagreements++;
            }
        }
        assertTrue("this case is only evidence if the two matrices disagree somewhere,"
                + " and on a sphere they disagree almost everywhere", disagreements > 10);

        // Positions rode L, which is the other half of the same bake.
        for (int i = 0; i < scaled.vertexCount(); ++i) {
            assertEquals("position " + i + " X", source.positions[i * 3] * 4f,
                    scaled.positions[i * 3], 1e-4f);
            assertEquals("position " + i + " Y", source.positions[i * 3 + 1],
                    scaled.positions[i * 3 + 1], 1e-5f);
        }
        assertOutwardWinding(scaled);
    }

    /**
     * `FSR1C-C1-06`, restated on a shape where perpendicularity is checkable.
     *
     * <p>A box's crease policy gives every corner one normal per face, so each
     * one must be exactly perpendicular to the triangles it belongs to. That
     * stays true through a bake — a rotation carries normals and faces
     * together — and it is the property a wrong normal matrix destroys wherever
     * the scale is not aligned with the face.
     */
    @Test
    public void fsr1cC1_06_bakedNormalsStayPerpendicularToTheirFaces() {
        applyBox(1.0, 2.0, 4.0);
        applyPlacement(0.0, 0.0, 0.0, 35.0, -20.0, 55.0, 2.0, 3.0, 0.5);
        final GlbDocument.Body body = activeBody(export());
        assertNormalsPerpendicularToTheirFaces(body);
        assertOutwardWinding(body);
    }

    @Test
    public void fsr1cC1_07_positionBoundsAreRecomputedFromTheBakedVertices() {
        applyBox(1.0, 2.0, 4.0);
        applyPlacement(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 2.0, 3.0, 0.5);
        final GlbDocument.Body body = activeBody(export());

        // The reader recomputes the bounds from the data and reports a problem
        // if the accessor disagrees, so `activeBody`'s clean-validate already
        // covers this. Restated explicitly because the failure it guards — a
        // min/max carried over from before the bake — is silent in every viewer
        // that does not cull or frame by them.
        final float[] bounds = body.localBounds();
        assertArrayEquals("the declared min is the baked min",
                new float[]{bounds[0], bounds[1], bounds[2]}, body.declaredMin, 1e-6f);
        assertArrayEquals("the declared max is the baked max",
                new float[]{bounds[3], bounds[4], bounds[5]}, body.declaredMax, 1e-6f);
        assertEquals("and they describe the baked size, not the authored one",
                6.0f, body.declaredMax[1] - body.declaredMin[1], 1e-4f);
    }

    @Test
    public void fsr1cC1_09_eachBodyBakesItsOwnTransformIndependently() {
        applyBox(1.0, 1.0, 1.0);
        applyPlacement(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
        final long first = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneActiveBodyId());
        final long second = addBody();
        applyBox(1.0, 1.0, 1.0);
        applyPlacement(5.0, 0.0, 0.0, 0.0, 0.0, 0.0, 3.0, 7.0, 0.25);

        final GlbDocument document = GlbDocument.parse(export());
        assertEquals("no structural problem: " + document.problemSummary(),
                0, document.validate().size());
        final GlbDocument.Body unscaled = named(document, "Body_" + first);
        final GlbDocument.Body scaled = named(document, "Body_" + second);

        final float[] a = unscaled.localBounds();
        assertEquals("the unscaled body is still a 1 m cube", 1.0f, a[3] - a[0], 1e-4f);
        assertEquals(1.0f, a[4] - a[1], 1e-4f);
        final float[] b = scaled.localBounds();
        assertEquals("and the scaled body baked ONLY its own scale",
                3.0f, b[3] - b[0], 1e-4f);
        assertEquals(7.0f, b[4] - b[1], 1e-4f);
        assertEquals(0.25f, b[5] - b[2], 1e-4f);
        assertArrayEquals("with its own translation still on its own node",
                new float[]{5.0f, 0.0f, 0.0f}, scaled.translation, 1e-5f);
        assertArrayEquals("and the other body untouched at the origin",
                new float[]{0.0f, 0.0f, 0.0f}, unscaled.translation, 1e-5f);
    }

    @Test
    public void fsr1cC1_11_thesameProjectStillExportsByteIdentically() {
        applyBox(1.0, 2.0, 4.0);
        applyPlacement(1.5, -0.5, 2.25, 370.0, 30.0, 12.25, 1.25, 2.0, 0.5);
        assertArrayEquals("baking must not make the export non-deterministic",
                export(), export());
    }

    @Test
    public void fsr1cC1_12_metresAndTheUpAxisAreUnchangedByTheBake() {
        applyBox(1.0, 2.0, 4.0);
        applyPlacement(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
        final float[] bounds = activeBody(export()).localBounds();
        assertEquals("width on X, in metres", 1.0f, bounds[3] - bounds[0], 1e-5f);
        assertEquals("height on Y — still the up-axis assertion",
                2.0f, bounds[4] - bounds[1], 1e-5f);
        assertEquals("depth on Z", 4.0f, bounds[5] - bounds[2], 1e-5f);
        assertEquals("an identity placement bakes the identity",
                -0.5f, bounds[0], 1e-5f);
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

        final List<String> places = new ArrayList<>();
        for (GlbDocument.Body body : document.bodies()) {
            final String key = Arrays.toString(body.translation);
            assertFalse("six differently placed bodies must not share a position: " + key,
                    places.contains(key));
            places.add(key);
            assertTrue("every body carries geometry", body.triangleCount() > 0);
        }
    }

    @Test
    public void e2er1c02_theFixturesTranslationsReachTheNodeAndItsScalesReachTheGeometry() {
        openSample(SAMPLE_CONSTRUCTION);
        final GlbDocument document = GlbDocument.parse(export());
        final long[] ids = sceneBodyIds();

        // The fixture places body i at (0.5i, -0.25i, 1.25i) with scale
        // (1 + 0.25i, 2, 0.5) and rotation (370, -45.5, 12.25i) — every value
        // asymmetric on purpose, so no axis can stand in for another. Under
        // ARCH-OWNER-07 the translation is the only one of the three that stays
        // on the node; the other two are in the vertices.
        for (int i = 0; i < ids.length; ++i) {
            final int n = (int) ids[i];
            final GlbDocument.Body body = document.bodies().get(i);
            final String who = "body " + n;
            assertArrayEquals(who + " keeps its authored position on the node",
                    new float[]{0.5f * n, -0.25f * n, 1.25f * n}, body.translation, 1e-5f);
            assertFalse(who + " must carry no node rotation", body.hasRotation);
            assertFalse(who + " must carry no node scale", body.hasScale);
            assertFalse(who + " must carry no node matrix", body.hasMatrix);

            // The scale reached the geometry instead. Every body is scaled 2x
            // on Y and 0.5x on Z, and each is a different shape, so the exact
            // extents differ — but a body whose scale had been dropped rather
            // than baked would have a Y extent no larger than its authored
            // dimension, and the fixture's tallest authored dimension is 2 m.
            final float[] bounds = body.localBounds();
            assertTrue(who + " has real baked extent",
                    bounds[3] - bounds[0] > 0f && bounds[4] - bounds[1] > 0f);
        }

        // The plain proof that the scale is baked: the fixture's Box is body 1,
        // authored 2 x 1 x 0.5 m with scale (1.25, 2, 0.5) and a rotation. Its
        // baked bounding box cannot be the authored one — 1 m on Y doubled is
        // 2 m before the rotation even spreads it.
        final GlbDocument.Body box = document.bodies().get(0);
        final float[] boxBounds = box.localBounds();
        assertTrue("the box's baked Y extent must exceed its authored 1 m",
                boxBounds[4] - boxBounds[1] > 1.5f);
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
        //
        // It is placed with a rotation, and under ARCH-OWNER-07 that rotation
        // is BAKED, so its axis-aligned bounds are no longer the fixture's
        // authored ones. The assertions are therefore made on properties a
        // rotation cannot change — the triangle count, the four distinct
        // corners and the longest edge between them — plus the two things the
        // bake must have changed and the one thing it must not have become.
        final GlbDocument.Body sculpted = document.bodies().get(1);
        assertEquals("a closed tetrahedron is four triangles", 4, sculpted.triangleCount());
        final List<String> corners = new ArrayList<>();
        double longestEdge = 0.0;
        for (int i = 0; i < sculpted.vertexCount(); ++i) {
            final float[] a = sculpted.localVertex(i);
            final String key = Math.round(a[0] * 1e4) + "," + Math.round(a[1] * 1e4) + ","
                    + Math.round(a[2] * 1e4);
            if (!corners.contains(key)) {
                corners.add(key);
            }
            for (int j = 0; j < sculpted.vertexCount(); ++j) {
                final float[] b = sculpted.localVertex(j);
                longestEdge = Math.max(longestEdge, Math.sqrt(
                        (a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1])
                                + (a[2] - b[2]) * (a[2] - b[2])));
            }
        }
        assertEquals("a tetrahedron has four distinct corners", 4, corners.size());
        // |BD| between the fixture's (1.5,0,0) and (0.25,0.5,1.75), which a
        // rigid rotation preserves exactly.
        assertEquals("the shape itself is unchanged by the bake",
                2.2079, longestEdge, 1e-3);

        final float[] bounds = sculpted.localBounds();
        // A 1.5 m sphere would be centred and span 1.5 on every axis. This does
        // not: the exporter did not fall back to the Construction Source.
        assertTrue("the sculpted body must not be its Construction sphere",
                Math.abs((bounds[3] - bounds[0]) - 1.5f) > 1e-2f
                        || Math.abs(bounds[0] + 0.75f) > 1e-2f);
        // And the rotation really was baked: the authored, unrotated mesh would
        // sit exactly in (0,0,0)..(1.5,1.25,1.75).
        assertTrue("the placement rotation must be in the vertices now",
                Math.abs(bounds[0]) > 1e-3f || Math.abs(bounds[3] - 1.5f) > 1e-3f);
        assertFalse("and no node rotation is left behind", sculpted.hasRotation);

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
            // UNCHANGED is a success here, not a refusal: a case that asks for
            // the placement the body already has — an identity, most often —
            // gets told so, and what matters to every assertion downstream is
            // that the nine values ARE these nine afterwards.
            final int status =
                    NativeViewport.applyBoxTransform(px, py, pz, rx, ry, rz, sx, sy, sz);
            assertTrue("the placement was refused: status " + status,
                    status == NativeViewport.APPLY_APPLIED
                            || status == NativeViewport.APPLY_UNCHANGED);
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

    /**
     * Every vertex normal is perpendicular to the triangles it belongs to.
     *
     * <p>The property that separates a correct normal bake from a plausible
     * one. A normal carried by {@code L} rather than by
     * {@code transpose(inverse(L))} still points roughly outward and still
     * normalises to unit length, so neither of those checks would notice; what
     * it stops being is PERPENDICULAR to its own surface, and that is what
     * shading actually depends on.
     *
     * <p>Only sound where the crease policy has already split hard edges — a
     * box after {@code buildRenderMesh} has one normal per face per corner — so
     * it is used on a box.
     */
    private static void assertNormalsPerpendicularToTheirFaces(GlbDocument.Body body) {
        for (int t = 0; t < body.triangleCount(); ++t) {
            final int[] corner = {body.indices[t * 3], body.indices[t * 3 + 1],
                    body.indices[t * 3 + 2]};
            final float[] a = body.localVertex(corner[0]);
            final float[] b = body.localVertex(corner[1]);
            final float[] c = body.localVertex(corner[2]);
            final float[] face = {
                    (b[1] - a[1]) * (c[2] - a[2]) - (b[2] - a[2]) * (c[1] - a[1]),
                    (b[2] - a[2]) * (c[0] - a[0]) - (b[0] - a[0]) * (c[2] - a[2]),
                    (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])};
            final float length = (float) Math.sqrt(
                    face[0] * face[0] + face[1] * face[1] + face[2] * face[2]);
            assertTrue("triangle " + t + " is degenerate", length > 1e-6f);
            for (int index : corner) {
                final float dot = (face[0] * body.normals[index * 3]
                        + face[1] * body.normals[index * 3 + 1]
                        + face[2] * body.normals[index * 3 + 2]) / length;
                assertEquals("vertex " + index + " normal is not perpendicular to triangle "
                        + t + " — it was carried by the wrong matrix", 1.0f, dot, 2e-3f);
            }
        }
    }

    /** Unit vector, or null when the input is too short to have a direction. */
    private static float[] normalized(float x, float y, float z) {
        final float length = (float) Math.sqrt(x * x + y * y + z * z);
        return length < 1e-6f ? null : new float[]{x / length, y / length, z / length};
    }

    private static GlbDocument.Body named(GlbDocument document, String name) {
        for (GlbDocument.Body body : document.bodies()) {
            if (name.equals(body.name)) {
                return body;
            }
        }
        throw new AssertionError(name + " is not in the export");
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
