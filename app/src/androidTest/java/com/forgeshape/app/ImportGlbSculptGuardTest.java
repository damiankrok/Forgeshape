package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.net.Uri;
import android.util.Log;
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
 * `D1-02..D1-10` (FUNCTION-COUNCIL-C1): Import GLB is not a Sculpt act.
 *
 * <p>Import creates bodies, makes the first of them active and records one
 * Construction step. None of that is legal while sculpting: the active body IS
 * the sculpt target, and Construction history is refused in Sculpt. So the
 * control is withdrawn in Sculpt, a picker result that arrives there anyway
 * (the picker is another app, and it answers whenever it answers) is refused
 * before the file is read, and the native import refuses on its own, under the
 * same lock as the commit, for any caller that got past both.
 *
 * <p>A refusal is proved NEUTRAL against one snapshot of everything a
 * successful import would have moved — the mode, the active body, the sculpt
 * target, the body ids, the sculpt revision and history, the Construction
 * history, the fingerprint, the encoded project and the active body's durable
 * flags — taken after a real stroke so the sculpt side has something to lose.
 *
 * <p>Each case ends with the same file imported legally in Construction, so a
 * guard that refused everything could not pass.
 */
@RunWith(AndroidJUnit4.class)
public final class ImportGlbSculptGuardTest {

    private static final String TAG = "ForgeShapeD1";

    /** Six top-level mesh nodes; the fixture the other import suites use. */
    private static final String SIX_NODE_GLB = "glb/construction_sentinel.glb";
    private static final int SIX_NODE_OBJECTS = 6;

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private final File scratch = new File(context().getCacheDir(), "d1-import-sculpt-test");

    /** The project as the case found it; restored through the ordinary load. */
    private byte[] baselineProject;

    private static Context context() {
        return InstrumentationRegistry.getInstrumentation().getTargetContext();
    }

    @Before
    public void setUp() {
        clearAllProjectFiles();
        deleteRecursively(scratch);
        assertTrue(scratch.mkdirs());
        resetToBaselineConstruction(rule.getScenario());
        baselineProject = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.encodeProject());
    }

    @After
    public void tearDown() {
        clearAllProjectFiles();
        deleteRecursively(scratch);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            if (workspace.projectPopover().isOpen()) {
                workspace.findViewById(R.id.project_actions_button).performClick();
            }
            NativeViewport.enterConstructionMode();
            workspace.uiState().recordRecoveryResolved();
            if (baselineProject != null) {
                NativeViewport.loadProject(baselineProject);
            }
            workspace.onNativeStateChanged();
            return null;
        });
        resetToBaselineConstruction(rule.getScenario());
    }

    // -----------------------------------------------------------------------
    // D1-02: the control is withdrawn in Sculpt and comes back in Construction
    // -----------------------------------------------------------------------

    @Test
    public void d1_02_importGlbIsWithdrawnInSculptAndReturnsInConstruction() {
        openProjectSurface();
        assertTrue("precondition: Import GLB is offered in Construction", importOffered());
        closeProjectSurface();

        startSculptingAConstructionSphere();
        openProjectSurface();
        final boolean offeredInSculpt = importOffered();
        final boolean labelInSculpt = importSectionLabelShown();
        closeProjectSurface();
        Log.i(TAG, "D1_OBSERVED_UI importOfferedInSculpt=" + offeredInSculpt
                + " importLabelShownInSculpt=" + labelInSculpt);
        assertFalse("D1-02: Import GLB is not offered while sculpting", offeredInSculpt);
        assertFalse("D1-02: nor is its section label left standing alone", labelInSculpt);

        backToConstruction();
        openProjectSurface();
        assertTrue("D1-02: Import GLB is offered again in Construction", importOffered());
        closeProjectSurface();
    }

    // -----------------------------------------------------------------------
    // D1-03, D1-05..D1-10: a stale picker result in Sculpt, then a legal import
    // -----------------------------------------------------------------------

    @Test
    public void d1_03_aPickerResultArrivingInSculptIsRefusedNeutrallyThenImportsInConstruction() {
        startSculptingAConstructionSphere();
        final File file = writeFixture();
        final Snapshot before = snapshot();

        // The system picker's answer, delivered the way the Activity delivers
        // it -- after the user was already sculpting.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onOpenGlbDocumentChosen(Uri.fromFile(file));
            return null;
        });
        settleLayout();
        final Snapshot after = snapshot();
        Log.i(TAG, "D1_OBSERVED_PICKER before=" + before + " after=" + after);
        assertEquals("D1-03/05..09: a picker result in Sculpt changes nothing at all",
                before.toString(), after.toString());

        assertLegalImportAfterBackToConstruction(file);
    }

    // -----------------------------------------------------------------------
    // D1-04: below the chrome -- the bytes entry and the JNI entry point
    // -----------------------------------------------------------------------

    @Test
    public void d1_04_theNativeImportRefusesInSculptByNameAndChangesNothing() {
        startSculptingAConstructionSphere();
        final File file = writeFixture();
        final byte[] bytes = readFile(file);
        final Snapshot before = snapshot();

        // The bytes half, which a test (or any later caller) reaches without
        // the picker guard in front of it.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.applyImportedGlbBytes(bytes);
            return null;
        });
        settleLayout();
        final Snapshot afterBytes = snapshot();

        // And the JNI entry point itself, called directly.
        final int status = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.importGlbDurable(bytes));
        final String token = onWorkspace(rule.getScenario(),
                (activity, workspace) -> status >= NativeViewport.IMPORT_COMMIT_BASE
                        ? NativeViewport.glbCommitStatusToken(status)
                        : NativeViewport.glbImportStatusToken(status));
        settleLayout();
        final Snapshot afterJni = snapshot();
        Log.i(TAG, "D1_OBSERVED_NATIVE status=" + status + " token=" + token
                + " before=" + before + " afterBytes=" + afterBytes + " afterJni=" + afterJni);

        assertEquals("D1-04: the bytes entry changes nothing in Sculpt",
                before.toString(), afterBytes.toString());
        assertEquals("D1-04: the native import refuses in Sculpt, by name",
                "RefusedInSculpt", token);
        assertTrue("D1-04: as a PROJECT refusal, not a file refusal",
                status >= NativeViewport.IMPORT_COMMIT_BASE);
        assertEquals("D1-04: and the direct JNI call changes nothing either",
                before.toString(), afterJni.toString());

        assertLegalImportAfterBackToConstruction(file);
    }

    // -----------------------------------------------------------------------
    // Shared journey steps
    // -----------------------------------------------------------------------

    /**
     * D1-10: Back to Construction, then the SAME file through the same picker
     * path imports as it always has -- six Imported Mesh bodies, the first of
     * them active, one Construction step.
     */
    private void assertLegalImportAfterBackToConstruction(File file) {
        backToConstruction();
        assertEquals("precondition: back in Construction", NativeViewport.MODE_CONSTRUCTION,
                productMode());
        final long[] idsBefore = sceneBodyIds();
        final int undoBefore = constructionUndoDepth();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onOpenGlbDocumentChosen(Uri.fromFile(file));
            return null;
        });
        settleLayout();

        final long[] idsAfter = sceneBodyIds();
        assertEquals("D1-10: the same file imports its six objects in Construction",
                idsBefore.length + SIX_NODE_OBJECTS, idsAfter.length);
        assertEquals("D1-10: the bodies that were there are untouched and first",
                Arrays.toString(idsBefore),
                Arrays.toString(Arrays.copyOf(idsAfter, idsBefore.length)));
        final Long active = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneActiveBodyId());
        assertEquals("D1-10: the first imported body is active, as before",
                idsAfter[idsBefore.length], active.longValue());
        assertTrue("D1-10: and it is an Imported Mesh", onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneActiveBodyIsImported()));
        assertEquals("D1-10: the whole import is one Construction step", undoBefore + 1,
                constructionUndoDepth());
        assertEquals("D1-10: the mode is still Construction", NativeViewport.MODE_CONSTRUCTION,
                productMode());
    }

    /** A sphere at the origin, sculpted once, so every sculpt reader is non-trivial. */
    private void startSculptingAConstructionSphere() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertAccepted("the sphere", NativeViewport.applyConstructionSphere(2.0));
            assertAccepted("the pose", NativeViewport.applyBoxTransform(
                    0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0));
            NativeViewport.setSculptBrush(300.0, 1.0);
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.freeze_to_sculpt).performClick();
            return null;
        });
        settleLayout();
        assertEquals("precondition: Start Sculpting entered Sculpt", NativeViewport.MODE_SCULPT,
                productMode());
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        final int sculptUndo = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sculptUndoDepth());
        assertTrue("precondition: a real stroke is on the sculpt history", sculptUndo >= 1);
    }

    private void backToConstruction() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.back_to_construction).performClick();
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

    /** Whether the Import GLB row is on screen and would take a tap. */
    private boolean importOffered() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View row = workspace.findViewById(R.id.import_glb);
            assertNotNull("the import row exists in the view tree", row);
            return workspace.projectPopover().isOpen() && row.isShown() && row.isEnabled();
        });
    }

    private boolean importSectionLabelShown() {
        return onWorkspace(rule.getScenario(), (activity, workspace) ->
                workspace.projectPopover().importSectionLabel().isShown());
    }

    // -----------------------------------------------------------------------
    // The neutrality snapshot
    // -----------------------------------------------------------------------

    /** Everything a successful import would move, compared as one string. */
    private static final class Snapshot {
        int mode;
        long activeBodyId;
        long sculptTargetId;
        String bodyIds;
        long sculptRevision;
        int sculptHasEdits;
        int sculptUndoDepth;
        int constructionUndoDepth;
        long fingerprint;
        String projectDigest;
        boolean activeVisible;
        boolean activeLocked;

        @Override
        public String toString() {
            return "{mode=" + mode + " active=" + activeBodyId + " sculptTarget=" + sculptTargetId
                    + " ids=" + bodyIds + " sculptRevision=" + sculptRevision
                    + " sculptHasEdits=" + sculptHasEdits + " sculptUndo=" + sculptUndoDepth
                    + " constructionUndo=" + constructionUndoDepth
                    + " fingerprint=" + Long.toHexString(fingerprint)
                    + " project=" + projectDigest + " activeVisible=" + activeVisible
                    + " activeLocked=" + activeLocked + "}";
        }
    }

    private Snapshot snapshot() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final Snapshot s = new Snapshot();
            final double[] sculpt = new double[NativeViewport.SCULPT_STATE_SIZE];
            NativeViewport.sculptState(sculpt);
            s.mode = NativeViewport.productMode();
            s.activeBodyId = NativeViewport.sceneActiveBodyId();
            s.sculptTargetId = (long) sculpt[NativeViewport.SCULPT_OBJECT_ID];
            final long[] ids = new long[NativeViewport.sceneBodyCount()];
            NativeViewport.sceneBodyIds(ids);
            s.bodyIds = Arrays.toString(ids);
            s.sculptRevision = (long) sculpt[NativeViewport.SCULPT_REVISION];
            s.sculptHasEdits = (int) sculpt[NativeViewport.SCULPT_HAS_EDITS];
            s.sculptUndoDepth = NativeViewport.sculptUndoDepth();
            s.constructionUndoDepth = NativeViewport.constructionUndoDepth();
            s.fingerprint = NativeViewport.projectFingerprint();
            s.projectDigest = describeBytes(NativeViewport.encodeProject());
            s.activeVisible = NativeViewport.sceneBodyVisible(s.activeBodyId);
            s.activeLocked = NativeViewport.sceneBodyLocked(s.activeBodyId);
            return s;
        });
    }

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    private int productMode() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.productMode());
    }

    private int constructionUndoDepth() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.constructionUndoDepth());
    }

    private long[] sceneBodyIds() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final long[] ids = new long[NativeViewport.sceneBodyCount()];
            NativeViewport.sceneBodyIds(ids);
            return ids;
        });
    }

    private File writeFixture() {
        final File file = new File(scratch, "external.glb");
        try (FileOutputStream out = new FileOutputStream(file)) {
            out.write(readAsset(SIX_NODE_GLB));
        } catch (IOException e) {
            throw new AssertionError("could not write " + file, e);
        }
        return file;
    }

    private static void assertAccepted(String what, int status) {
        assertTrue(what + " must be accepted, not rejected (status " + status + ")",
                status == NativeViewport.APPLY_APPLIED
                        || status == NativeViewport.APPLY_UNCHANGED);
    }

    /** Length and FNV-1a 64 of a byte array: comparable and readable in a failure. */
    private static String describeBytes(byte[] bytes) {
        if (bytes == null) {
            return "null";
        }
        long hash = 0xcbf29ce484222325L;
        for (byte b : bytes) {
            hash ^= (b & 0xFF);
            hash *= 0x100000001b3L;
        }
        return bytes.length + ":" + Long.toHexString(hash);
    }

    private static void clearAllProjectFiles() {
        context().deleteFile(ProjectSlot.SLOT_FILE_NAME);
        context().deleteFile(ProjectCheckpoint.CHECKPOINT_FILE_NAME);
        ProjectCheckpoint.quarantineFile(context()).delete();
    }

    private static byte[] readAsset(String name) {
        // The INSTRUMENTATION context's assets: fixtures ship with the test APK.
        try (InputStream in = InstrumentationRegistry.getInstrumentation()
                .getContext().getAssets().open(name)) {
            return drain(in);
        } catch (IOException e) {
            throw new AssertionError("asset " + name + " must be packaged", e);
        }
    }

    private static byte[] readFile(File file) {
        try (InputStream in = new java.io.FileInputStream(file)) {
            return drain(in);
        } catch (IOException e) {
            throw new AssertionError("could not read " + file, e);
        }
    }

    private static byte[] drain(InputStream in) throws IOException {
        final ByteArrayOutputStream out = new ByteArrayOutputStream();
        final byte[] buffer = new byte[8192];
        int read;
        while ((read = in.read(buffer)) > 0) {
            out.write(buffer, 0, read);
        }
        return out.toByteArray();
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
