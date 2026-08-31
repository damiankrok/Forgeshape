package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.view.View;

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
import java.util.Arrays;

/**
 * `GLBIR0-01..20` and `E2E-GLBIR0-01..08`: the diagnostic imported mesh
 * preview, and the world-geometry roundtrip it exists to measure.
 *
 * <h2>The question these cases answer</h2>
 *
 * <p>The owner saw a discrepancy between the ForgeShape scene and Blender that
 * the corrected node scale of 1/1/1 did not explain. Three things could produce
 * that: ForgeShape's exporter is wrong, a reader is wrong, or the external tool
 * presents the same geometry differently. These cases can separate the first
 * from the other two, which is the split that decides whether ForgeShape has a
 * defect — and that is all they claim.
 *
 * <h2>Why the comparison is worth anything</h2>
 *
 * <p>The expected side is re-derived from DOMAIN truth — the primitive
 * generator or the Frozen Sculpt Mesh, through the product's own render-mesh
 * derivation, placed by {@code modelMatrix()}. It is not the exporter's
 * captured arrays. The actual side is the real bytes, parsed by
 * {@code forgeshape_gltf_import}, which shares no line with the writer. If the
 * exporter captured the wrong geometry, the expected side still holds the right
 * geometry and the comparison fails — which is the point.
 *
 * <h2>What the preview is not</h2>
 *
 * <p>Session-only, and never project truth. Several cases below exist purely to
 * hold that boundary: no body appears, no history moves, no `.forge` byte
 * changes, no fingerprint moves, and it is gone when the process is.
 */
@RunWith(AndroidJUnit4.class)
public final class GlbImportPreviewTest {

    private static final String SENTINEL_CONSTRUCTION = "glb/construction_sentinel.glb";
    private static final String SENTINEL_SCULPT = "glb/sculpt_sentinel.glb";
    private static final String FORGE_CONSTRUCTION = "forge/construction_multibody_v1.forge";
    private static final String FORGE_SCULPT = "forge/sculpt_mixed_v1.forge";

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private final File scratch = new File(context().getCacheDir(), "glb-import-test");

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
    // GLBIR0-01/02: the committed sentinels parse
    // -----------------------------------------------------------------------

    @Test
    public void glbir0_01_theCorrectedConstructionSentinelParses() {
        final byte[] bytes = readAsset(SENTINEL_CONSTRUCTION);
        assertEquals("the corrected Construction sentinel must parse",
                NativeViewport.IMPORT_OK, importPreview(bytes));
        final int[] counts = previewCounts();
        assertEquals("six bodies became six meshes", 6, counts[0]);
        assertTrue("and they carry real geometry", counts[1] > 0 && counts[2] > 0);
        assertTrue(loaded());
    }

    @Test
    public void glbir0_02_theCorrectedSculptSentinelParses() {
        final byte[] bytes = readAsset(SENTINEL_SCULPT);
        assertEquals(NativeViewport.IMPORT_OK, importPreview(bytes));
        final int[] counts = previewCounts();
        assertEquals("the Sculpt fixture's two bodies", 2, counts[0]);
        // 12 box triangles plus the sculpted tetrahedron's 4.
        assertEquals("the sculpted tetrahedron is in there, not a sphere", 16, counts[2]);
    }

    // -----------------------------------------------------------------------
    // GLBIR0-03/06/07: fail closed, by name
    // -----------------------------------------------------------------------

    @Test
    public void glbir0_03_containerValidationIsIndependentAndFailsClosed() {
        final byte[] good = readAsset(SENTINEL_CONSTRUCTION);

        assertEquals("empty bytes", refusal(new byte[0]), refusal(new byte[0]));
        assertTrue("empty bytes are refused", importPreview(new byte[0]) != NativeViewport.IMPORT_OK);
        assertTrue("null is refused", importPreview(null) != NativeViewport.IMPORT_OK);

        final byte[] notGlb = good.clone();
        notGlb[0] = 'X';
        assertTrue("a wrong magic is refused",
                importPreview(notGlb) != NativeViewport.IMPORT_OK);

        final byte[] truncated = Arrays.copyOf(good, good.length - 64);
        assertTrue("a truncated file is refused",
                importPreview(truncated) != NativeViewport.IMPORT_OK);

        final byte[] forge = readAsset(FORGE_CONSTRUCTION);
        assertTrue("a `.forge` project is not a GLB and is refused",
                importPreview(forge) != NativeViewport.IMPORT_OK);

        assertFalse("and after every refusal there is still no preview", loaded());
    }

    @Test
    public void glbir0_06and07_aRefusalNeverReplacesAnExistingPreview() {
        assertEquals(NativeViewport.IMPORT_OK, importPreview(readAsset(SENTINEL_CONSTRUCTION)));
        final int[] before = previewCounts();

        assertTrue(importPreview(new byte[]{1, 2, 3, 4}) != NativeViewport.IMPORT_OK);
        assertTrue("the good preview survives a refused load", loaded());
        assertArrayEquals("untouched, not half-replaced", before, previewCounts());
    }

    // -----------------------------------------------------------------------
    // GLBIR0-08/11: the preview is not project truth
    // -----------------------------------------------------------------------

    @Test
    public void glbir0_08_thePreviewOwnsNoBodyHistoryOrIdentity() {
        final long[] idsBefore = sceneBodyIds();
        final long activeBefore = activeBodyId();
        final long revisionBefore = meshRevision();
        final int undoBefore = undoDepth();

        assertEquals(NativeViewport.IMPORT_OK, importPreview(readAsset(SENTINEL_CONSTRUCTION)));
        setPreviewVisible(true);

        assertArrayEquals("the preview added no body", idsBefore, sceneBodyIds());
        assertEquals("nor changed which body is active", activeBefore, activeBodyId());
        assertEquals("nor published a revision", revisionBefore, meshRevision());
        assertEquals("nor recorded a history step", undoBefore, undoDepth());
        assertEquals("nor a redo", 0, redoDepth());
    }

    @Test
    public void glbir0_11_switchingSourceToImportedAndBackMutatesNothing() {
        edit(4.5, 2.25, 1.125);
        final double[] before = WorkspaceTestSupport.nativeSnapshot();
        final long fingerprintBefore = fingerprint();
        final byte[] projectBefore = encodeProject();

        assertEquals(NativeViewport.IMPORT_OK, importPreview(readAsset(SENTINEL_CONSTRUCTION)));
        setPreviewVisible(true);
        assertTrue("the viewport is showing the imported file", previewVisible());
        settleLayout();

        setPreviewVisible(false);
        settleLayout();
        assertFalse(previewVisible());

        final String drift = WorkspaceTestSupport.describeSnapshotDifference(
                before, WorkspaceTestSupport.nativeSnapshot());
        assertEquals("no observable native state moved:" + drift, "", drift);
        assertEquals("the project fingerprint is unchanged", fingerprintBefore, fingerprint());
        assertArrayEquals("and the project re-encodes to the same bytes",
                projectBefore, encodeProject());
    }

    // -----------------------------------------------------------------------
    // GLBIR0-09/10: persistence is untouched
    // -----------------------------------------------------------------------

    @Test
    public void glbir0_09_aManualSaveIsUnchangedByThePreview() {
        edit(3.5, 1.75, 0.875);
        final byte[] withoutPreview = encodeProject();

        assertEquals(NativeViewport.IMPORT_OK, importPreview(readAsset(SENTINEL_CONSTRUCTION)));
        setPreviewVisible(true);

        assertArrayEquals("the bytes a Save would write do not include the preview",
                withoutPreview, encodeProject());

        // And a real Save through the production path writes exactly those.
        final File destination = new File(scratch, "with-preview.forge");
        final byte[] bytes = encodeProject();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onSaveCopyRequestedForTest(bytes);
            workspace.onCreateProjectDocumentChosen(Uri.fromFile(destination));
            return null;
        });
        settleLayout();
        assertArrayEquals(withoutPreview, readFile(destination));
        assertEquals("which the real decoder still accepts", NativeViewport.PROJECT_OK,
                NativeViewport.validateProject(readFile(destination)));
    }

    @Test
    public void glbir0_10_theAutosaveFingerprintDoesNotMoveForThePreview() {
        edit(2.5, 1.25, 0.625);
        final long fingerprintBefore = fingerprint();

        assertEquals(NativeViewport.IMPORT_OK, importPreview(readAsset(SENTINEL_CONSTRUCTION)));
        assertEquals("loading a preview is not a project change",
                fingerprintBefore, fingerprint());
        setPreviewVisible(true);
        assertEquals("nor is showing it", fingerprintBefore, fingerprint());
        setPreviewVisible(false);
        clearPreview();
        assertEquals("nor is clearing it", fingerprintBefore, fingerprint());

        // Nothing above may have created a checkpoint either.
        assertFalse("no recovery checkpoint was written for a preview",
                context().getFileStreamPath(ProjectCheckpoint.CHECKPOINT_FILE_NAME).exists());
    }

    // -----------------------------------------------------------------------
    // GLBIR0-12: Clear releases and restores
    // -----------------------------------------------------------------------

    @Test
    public void glbir0_12_clearReleasesThePreviewAndRestoresTheSource() {
        assertEquals(NativeViewport.IMPORT_OK, importPreview(readAsset(SENTINEL_CONSTRUCTION)));
        setPreviewVisible(true);
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onClearImportedPreviewRequested();
            return null;
        });
        settleLayout();

        assertFalse("nothing is loaded", loaded());
        assertFalse("and nothing is shown", previewVisible());
        assertArrayEquals("the counts are zero", new int[]{0, 0, 0}, previewCounts());
        // The workspace came back: the editing chrome is on screen again.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("the trailing host is back", View.VISIBLE,
                    WorkspaceTestSupport.trailingHost(workspace).getVisibility());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // GLBIR0-13/14/15/16: the roundtrip, on device
    // -----------------------------------------------------------------------

    @Test
    public void glbir0_13and15and16_theConstructionSourceRoundtripsEquivalent() {
        openForgeSample(FORGE_CONSTRUCTION);
        final String report = roundtripReport();
        assertTrue("the six-body Construction project must roundtrip equivalent:\n" + report,
                report.contains("verdict=ROUNDTRIP_EQUIVALENT"));
        assertEquals("six bodies compared", 6, valueOf(report, "source_bodies="), 0.0);
        assertEquals("against six imported meshes", 6, valueOf(report, "imported_meshes="), 0.0);
        // GLBIR0-15/16: the deltas are reported, and are float32 noise.
        final double position = valueOf(report, "max_position_delta_m=");
        final double normal = valueOf(report, "max_normal_delta_deg=");
        assertTrue("the max world-position delta must be float32 quantization, was " + position,
                position < 1e-3);
        assertTrue("the max world-normal delta must be tiny, was " + normal, normal < 0.25);
        assertTrue("every body must be within tolerance:\n" + report,
                !report.contains("within=NO"));
        assertTrue("and their topology identical:\n" + report,
                !report.contains("topology=DIFFER"));
    }

    @Test
    public void glbir0_14_theSculptSourceRoundtripsEquivalentAsSculpt() {
        openForgeSample(FORGE_SCULPT);
        assertEquals("the fixture reopens in Sculpt", NativeViewport.MODE_SCULPT,
                onWorkspace(rule.getScenario(),
                        (activity, workspace) -> NativeViewport.productMode()).intValue());

        final String report = roundtripReport();
        assertTrue("the Sculpt project must roundtrip equivalent:\n" + report,
                report.contains("verdict=ROUNDTRIP_EQUIVALENT"));
        assertTrue("and the compared representation must be the sculpt mesh:\n" + report,
                report.contains("source=sculpt"));
        assertTrue("its companion is still Construction:\n" + report,
                report.contains("source=construction"));
        assertTrue(valueOf(report, "max_position_delta_m=") < 1e-3);
    }

    @Test
    public void glbir0_13_theComparisonCanAlsoFail() {
        // The diagnostic is only evidence if a real disagreement produces a
        // mismatch. Comparing the live project against a DIFFERENT project's
        // file is exactly that.
        openForgeSample(FORGE_CONSTRUCTION);
        final String report = compareReport(readAsset(SENTINEL_SCULPT));
        assertTrue("a file from another project must not read as equivalent:\n" + report,
                report.contains("verdict=ROUNDTRIP_MISMATCH")
                        || report.contains("verdict=ROUNDTRIP_NOT_COMPARABLE"));
    }

    @Test
    public void glbir0_13_theCommittedSentinelStillMatchesItsSourceProject() {
        // The strongest form of the owner's question: the exact bytes in the
        // evidence bundle, against the exact project they were exported from.
        openForgeSample(FORGE_CONSTRUCTION);
        final String report = compareReport(readAsset(SENTINEL_CONSTRUCTION));
        assertTrue("the committed Construction sentinel must still match its source:\n" + report,
                report.contains("verdict=ROUNDTRIP_EQUIVALENT"));
    }

    // -----------------------------------------------------------------------
    // GLBIR0-17/18/19/20: the picker, the controls, the boundary
    // -----------------------------------------------------------------------

    @Test
    public void glbir0_17_theOpenIntentAsksForAFileAndACancelIsANoOp() {
        final Intent intent = ProjectTransfer.openGlbDocumentIntent();
        assertEquals(Intent.ACTION_OPEN_DOCUMENT, intent.getAction());
        assertTrue(intent.hasCategory(Intent.CATEGORY_OPENABLE));
        assertNotNull(intent.getType());

        final double[] before = WorkspaceTestSupport.nativeSnapshot();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onOpenGlbDocumentChosen(null);
            return null;
        });
        settleLayout();
        assertFalse("a cancel loads nothing", loaded());
        assertEquals("and changes nothing", "", WorkspaceTestSupport.describeSnapshotDifference(
                before, WorkspaceTestSupport.nativeSnapshot()));
    }

    @Test
    public void glbir0_17_anUnreadableSelectionIsBoundedAndNonDestructive() {
        final double[] before = WorkspaceTestSupport.nativeSnapshot();
        final File missing = new File(scratch, "not-there.glb");
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onOpenGlbDocumentChosen(Uri.fromFile(missing));
            return null;
        });
        settleLayout();
        assertFalse(loaded());
        assertEquals("", WorkspaceTestSupport.describeSnapshotDifference(
                before, WorkspaceTestSupport.nativeSnapshot()));
    }

    @Test
    public void glbir0_18_theControlsHaveSemanticIdsAndMeetTheTouchFloor() {
        openProjectSurface();
        // With nothing loaded, only the one control that can succeed is drawn.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View open = workspace.findViewById(R.id.import_glb_preview);
            assertNotNull("the check control exists by id", open);
            assertEquals(View.VISIBLE, open.getVisibility());
            assertEquals("Show is absent with nothing loaded", View.GONE,
                    workspace.findViewById(R.id.toggle_imported_preview).getVisibility());
            assertEquals("and so is Clear", View.GONE,
                    workspace.findViewById(R.id.clear_imported_preview).getVisibility());
            return null;
        });

        assertEquals(NativeViewport.IMPORT_OK, importPreview(readAsset(SENTINEL_CONSTRUCTION)));
        closeProjectSurface();
        openProjectSurface();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final float density = activity.getResources().getDisplayMetrics().density;
            final int floor = Math.round(48f * density);
            for (int id : new int[]{R.id.import_glb_preview, R.id.toggle_imported_preview,
                    R.id.clear_imported_preview}) {
                final View control = workspace.findViewById(id);
                assertEquals(activity.getResources().getResourceEntryName(id) + " is drawn",
                        View.VISIBLE, control.getVisibility());
                assertTrue(activity.getResources().getResourceEntryName(id)
                                + " must meet the 48 dp floor, was " + control.getHeight(),
                        control.getHeight() >= floor - 1);
                assertTrue("and must be at least 48 dp wide", control.getWidth() >= floor - 1);
            }
            return null;
        });
    }

    @Test
    public void glbir0_19_previewModeDrawsNoControlThatCannotSucceed() {
        assertEquals(NativeViewport.IMPORT_OK, importPreview(readAsset(SENTINEL_CONSTRUCTION)));
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onToggleImportedPreviewRequested();
            return null;
        });
        settleLayout();
        assertTrue(previewVisible());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            // Nothing that edits a body may be on screen: the preview has no
            // body to edit and the model it would edit is not visible.
            assertEquals("the trailing host, which carries the Tool Rail, the transform"
                            + " selectors and the precision trigger, is withdrawn",
                    View.GONE, WorkspaceTestSupport.trailingHost(workspace).getVisibility());
            assertEquals("Undo/Redo are withdrawn", View.GONE,
                    workspace.historyGroup().getVisibility());
            // `isShown()` rather than `getVisibility()`, deliberately: a leaf
            // keeps its own VISIBLE flag when an ancestor goes away, and the
            // question here is whether the user can see and press the control,
            // which is what `isShown()` answers.
            for (int id : new int[]{R.id.apply_shape, R.id.apply_transform, R.id.add_body,
                    R.id.freeze_to_sculpt, R.id.tool_rail_shape, R.id.tool_rail_place}) {
                final View control = workspace.findViewById(id);
                if (control != null) {
                    assertFalse(activity.getResources().getResourceEntryName(id)
                                    + " must not be on screen over an imported preview",
                            control.isShown());
                }
            }
            return null;
        });

        // And switching back restores the accepted workspace.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onToggleImportedPreviewRequested();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(View.VISIBLE,
                    WorkspaceTestSupport.trailingHost(workspace).getVisibility());
            assertEquals(View.VISIBLE, workspace.historyGroup().getVisibility());
            return null;
        });
    }

    @Test
    public void glbir0_20_noInterchangeScopeBeyondTheDiagnosticIsOffered() {
        openProjectSurface();
        final StringBuilder visible = new StringBuilder();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            collectText(workspace.projectPopover(), visible);
            return null;
        });
        final String text = visible.toString().toLowerCase(java.util.Locale.US);
        for (String forbidden : new String[]{"obj", "fbx", "stl", "collada", "dae", "usdz",
                "material", "texture", "animation"}) {
            assertFalse("the project surface must not offer '" + forbidden + "': " + text,
                    text.contains(forbidden));
        }
        // GLBIR1-21. The action is named for what the user does — Import GLB —
        // and every surrounding word says PREVIEW, which is what they get.
        // Neither production import nor a second interchange format appears.
        assertTrue("Import GLB is offered: " + text, text.contains("import glb"));
        assertTrue("and it is named as a preview: " + text, text.contains("preview"));
    }

    // -----------------------------------------------------------------------
    // E2E-GLBIR0-08: a fresh process starts with no preview
    // -----------------------------------------------------------------------

    @Test
    public void e2eGlbir0_08_aPreviewNeverSurvivesIntoProjectState() {
        assertEquals(NativeViewport.IMPORT_OK, importPreview(readAsset(SENTINEL_CONSTRUCTION)));
        setPreviewVisible(true);

        // A preview is session state and is deliberately not part of anything
        // that outlives the process. Proved here as: the project bytes carry
        // nothing of it, and clearing leaves the project exactly as it was —
        // the process-death half is structural, because nothing ever writes it
        // anywhere.
        final byte[] project = encodeProject();
        clearPreview();
        assertArrayEquals(project, encodeProject());
        assertFalse(loaded());
        assertFalse("no `.forge` slot was created by any of this",
                context().getFileStreamPath(ProjectSlot.SLOT_FILE_NAME).exists());
    }

    // -----------------------------------------------------------------------
    // Evidence: the two roundtrip reports the owner-facing bundle carries
    // -----------------------------------------------------------------------
    //
    // Ordinary cases that additionally LEAVE their report where
    // scripts\run-glb-import-evidence.ps1 can pull it. The external files
    // directory, not the cache, because the cache is wiped between cases.

    @Test
    public void glbir0_evidenceConstructionRoundtrip() {
        openForgeSample(FORGE_CONSTRUCTION);
        final String report = roundtripReport();
        assertTrue("a report is only evidence if it is a verdict:\n" + report,
                report.contains("verdict=ROUNDTRIP_EQUIVALENT"));
        writeReport("construction_roundtrip.txt", report);
    }

    @Test
    public void glbir0_evidenceSculptRoundtrip() {
        openForgeSample(FORGE_SCULPT);
        final String report = roundtripReport();
        assertTrue("a report is only evidence if it is a verdict:\n" + report,
                report.contains("verdict=ROUNDTRIP_EQUIVALENT"));
        writeReport("sculpt_roundtrip.txt", report);
    }

    @Test
    public void glbir0_evidenceCommittedSentinelComparison() {
        openForgeSample(FORGE_CONSTRUCTION);
        final StringBuilder out = new StringBuilder();
        out.append("# The COMMITTED sentinel bytes, compared with the project they\n");
        out.append("# were exported from. Not a fresh export: these are the files in\n");
        out.append("# artifacts/e2er1c/, read back by the independent parser.\n\n");
        out.append("## construction_sentinel.glb\n");
        out.append(compareReport(readAsset(SENTINEL_CONSTRUCTION)));
        openForgeSample(FORGE_SCULPT);
        out.append("\n## sculpt_sentinel.glb\n");
        out.append(compareReport(readAsset(SENTINEL_SCULPT)));
        writeReport("committed_sentinels.txt", out.toString());
    }

    /**
     * The Source and Imported views of the same project, from the SAME camera.
     *
     * <p>The camera is process-scoped native state and is not touched by
     * loading, showing or hiding a preview, so the two captures share it by
     * construction rather than by being re-aimed between them — and it is
     * pinned to a fixed pose first, so the pair is reproducible run to run.
     * That is what makes them comparable; nothing here overlays or blends the
     * two, and the machine geometry comparison above remains the authority.
     */
    @Test
    public void glbir0_evidenceSourceAndImportedScreenshots() {
        openForgeSample(FORGE_CONSTRUCTION);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("a fixed camera pose must be accepted",
                    NativeViewport.debugSetCameraPose(0.6f, 0.35f, 16.0f));
            return null;
        });
        settleLayout();
        settleFrames();
        capture("source_scene.png");

        assertEquals(NativeViewport.IMPORT_OK, importPreview(readAsset(SENTINEL_CONSTRUCTION)));
        setPreviewVisible(true);
        settleFrames();
        final float[] pose = new float[8];
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.debugCameraPose(pose);
            return null;
        });
        assertEquals("the camera must not have moved between the two captures",
                16.0f, pose[2], 1e-3f);
        capture("imported_preview.png");

        setPreviewVisible(false);
    }

    /** Lets the render thread present a few frames before a capture. */
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
        writeFile(new File(directory, name), text.getBytes(java.nio.charset.StandardCharsets.UTF_8));
    }

    // -----------------------------------------------------------------------
    // Shared machinery
    // -----------------------------------------------------------------------

    private int importPreview(final byte[] bytes) {
        final int status = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.importGlbPreview(bytes));
        settleLayout();
        return status;
    }

    private String refusal(byte[] bytes) {
        return String.valueOf(importPreview(bytes));
    }

    private void setPreviewVisible(final boolean visible) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.setGlbPreviewVisible(visible);
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
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
            final int[] counts = new int[3];
            NativeViewport.glbPreviewCounts(counts);
            return counts;
        });
    }

    private String roundtripReport() {
        final String report = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.glbRoundtripReport());
        assertNotNull(report);
        return report;
    }

    private String compareReport(final byte[] bytes) {
        final String report = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.glbCompareReport(bytes));
        assertNotNull(report);
        return report;
    }

    /** The number after `key` in a `key=value` report line. */
    static double valueOf(String report, String key) {
        final int at = report.indexOf(key);
        assertTrue("the report must carry " + key + ":\n" + report, at >= 0);
        int end = at + key.length();
        while (end < report.length() && report.charAt(end) != '\n' && report.charAt(end) != ' ') {
            end++;
        }
        return Double.parseDouble(report.substring(at + key.length(), end));
    }

    private void edit(final double w, final double h, final double d) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionBox(w, h, d);
            return null;
        });
        settleLayout();
    }

    private byte[] encodeProject() {
        final byte[] bytes = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.encodeProject());
        assertNotNull(bytes);
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

    /** Loads a golden-corpus `.forge` fixture through the production handler. */
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

    private static void collectText(View view, StringBuilder out) {
        if (view instanceof android.widget.TextView) {
            out.append(((android.widget.TextView) view).getText()).append(' ');
        }
        if (view.getContentDescription() != null) {
            out.append(view.getContentDescription()).append(' ');
        }
        if (view instanceof android.view.ViewGroup) {
            final android.view.ViewGroup group = (android.view.ViewGroup) view;
            for (int i = 0; i < group.getChildCount(); i++) {
                collectText(group.getChildAt(i), out);
            }
        }
    }

    static byte[] readAsset(String name) {
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
