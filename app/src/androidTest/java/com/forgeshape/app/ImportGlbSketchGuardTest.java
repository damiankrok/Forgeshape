package com.forgeshape.app;

import static com.forgeshape.app.SketchTestSupport.dragSketch;
import static com.forgeshape.app.SketchTestSupport.drawRectangleAndExtrude;
import static com.forgeshape.app.SketchTestSupport.onNativeStateChanged;
import static com.forgeshape.app.SketchTestSupport.selectTool;
import static com.forgeshape.app.SketchTestSupport.sketchEntityCount;
import static com.forgeshape.app.SketchTestSupport.sketchState;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.pm.ActivityInfo;
import android.net.Uri;
import android.os.SystemClock;
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
 * `IMPORT-GLB-SKETCH-GUARD-R1` (closes `CR-03`'s sketch half): Import GLB is a
 * scene mutation, and a live sketch is the exclusive authoring act until it is
 * finished or cancelled.
 *
 * <p>An import creates bodies, makes the first of them active and records one
 * Construction step. Under an open sketch that is a second act the user cannot
 * undo (Construction Undo is refused while sketching) underneath a volatile one.
 * So the Import row is absent while a sketch is Editing or Ready, a picker
 * result that arrives after a sketch began is refused before the file is read,
 * and the native import refuses on its own, under the lock of its commit, for
 * any caller that got past both -- by NAME (`RefusedInSketch`, project code
 * {@code IMPORT_COMMIT_BASE + 6}), never as a corrupt file.
 *
 * <p>A refusal is proved NEUTRAL against one snapshot of everything a successful
 * import would move AND the draft it must leave alone: mode, active body, body
 * ids, Construction Undo/Redo, the fingerprint, the encoded project, the sketch
 * state and its entity count. Each case ends with the SAME file imported legally
 * once the sketch is gone, so a guard that refused everything could not pass.
 *
 * <p>The support chooser without a sketch and an import on Home are OWNER
 * questions (`CR-03` Q2, Q3) and are deliberately not exercised here.
 */
@RunWith(AndroidJUnit4.class)
public final class ImportGlbSketchGuardTest {

    private static final String TAG = "ForgeShapeImportSketch";

    /** Six top-level mesh nodes; the fixture the other import suites use. */
    private static final String SIX_NODE_GLB = "glb/construction_sentinel.glb";
    private static final int SIX_NODE_OBJECTS = 6;

    /**
     * The project refusal's wire code, pinned here as a NUMBER on purpose: the
     * ordinals before it (`RefusedInSculpt` = 1005 included) must not move, and
     * this one is appended after them.
     */
    private static final int REFUSED_IN_SKETCH = NativeViewport.IMPORT_COMMIT_BASE + 6;

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private final File scratch = new File(context().getCacheDir(), "import-sketch-guard-test");
    private ModelingOwnerRig rig;
    private byte[] baselineProject;

    private static Context context() {
        return InstrumentationRegistry.getInstrumentation().getTargetContext();
    }

    @Before
    public void setUp() {
        rig = new ModelingOwnerRig(rule.getScenario(), "import-glb-sketch-guard", "IMPSK_DEVICE");
        clearAllProjectFiles();
        deleteRecursively(scratch);
        assertTrue(scratch.mkdirs());
        WorkspaceTestSupport.resetToBaselineConstruction(rule.getScenario());
        baselineProject = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.encodeProject());
    }

    @After
    public void tearDown() {
        WorkspaceTestSupport.releaseOrientation(rule.getScenario());
        settleLayout();
        clearAllProjectFiles();
        deleteRecursively(scratch);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.setProjectTransferHost(activity.projectTransferHost());
            if (workspace.projectPopover().isOpen()) {
                workspace.findViewById(R.id.project_actions_button).performClick();
            }
            NativeViewport.supportChooserCancel();
            NativeViewport.sketchCancel();
            NativeViewport.enterConstructionMode();
            workspace.uiState().recordRecoveryResolved();
            if (baselineProject != null) {
                NativeViewport.loadProject(baselineProject);
            }
            workspace.onNativeStateChanged();
            return null;
        });
        rig.restore();
    }

    // -----------------------------------------------------------------------
    // IMPSK-UI-01: the Import row is absent in Editing and in Ready, and back
    // when the sketch is gone -- the drawer opened by a REAL tap on the mark.
    // -----------------------------------------------------------------------

    @Test
    public void sk01_importRowIsAbsentWhileASketchIsEditingOrReady() {
        assertTrue("precondition: Import GLB is offered with no sketch", importOfferedByRealTap());

        beginSketchWithARectangle();
        assertEquals(NativeViewport.SKETCH_EDITING, sketchState());
        final boolean offeredEditing = importOfferedByRealTap();
        finishToReady();
        final boolean offeredReady = importOfferedByRealTap();
        Log.i(TAG, "IMPSK_OBSERVED_UI editing=" + offeredEditing + " ready=" + offeredReady);
        rig.fact("import_row_offered_editing", offeredEditing);
        rig.fact("import_row_offered_ready", offeredReady);
        assertFalse("Import GLB is not offered while a sketch is Editing", offeredEditing);
        assertFalse("Import GLB is not offered while a sketch is Ready", offeredReady);

        cancelSketch();
        assertTrue("Import GLB is offered again once the sketch is gone", importOfferedByRealTap());
    }

    // -----------------------------------------------------------------------
    // IMPSK-UI-02 / IMPORT-SK-06: the picker opened LEGALLY, then a sketch
    // began, then the picker answered. Refused, neutral and idempotent; the
    // same answer after Cancel imports.
    // -----------------------------------------------------------------------

    @Test
    public void sk02_aPickerResultArrivingAfterASketchBeganIsRefusedNeutrally() {
        final int[] pickerRequests = {0};
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.setProjectTransferHost(new RecordingTransferHost(pickerRequests));
            return null;
        });
        // The request is legal: real taps on the mark, then on the Import row.
        rig.touchId(R.id.project_actions_button);
        assertTrue("precondition: the Import row is offered", rig.shown(R.id.import_glb));
        rig.touchId(R.id.import_glb);
        assertEquals("the real tap asked for the system picker", 1, pickerRequests[0]);

        // While the picker is up, a sketch begins (the picker is another app,
        // and it answers whenever it answers).
        beginSketchWithARectangle();
        final File file = writeFixture();
        final Snapshot before = snapshot();
        for (int attempt = 0; attempt < 3; attempt++) {
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                workspace.onOpenGlbDocumentChosen(Uri.fromFile(file));
                return null;
            });
            settleLayout();
        }
        final Snapshot after = snapshot();
        Log.i(TAG, "IMPSK_OBSERVED_PICKER before=" + before + " after=" + after);
        rig.fact("late_picker_status", rig.statusLine());
        assertEquals("a late picker result (x3) under a sketch changes nothing at all",
                before.toString(), after.toString());
        rig.capture("sk02_late_picker_refused");

        cancelSketch();
        assertLegalImport(file);
    }

    // -----------------------------------------------------------------------
    // IMPORT-SK-01/02/06/07: below the chrome, in Editing and in Ready.
    // -----------------------------------------------------------------------

    @Test
    public void sk03_theNativeImportRefusesInEditingAndReadyByNameAndTheDraftSurvives() {
        final File file = writeFixture();
        final byte[] bytes = readFile(file);

        beginSketchWithARectangle();
        assertRefusedEverywhere("Editing", bytes);
        finishToReady();
        assertEquals(NativeViewport.SKETCH_READY, sketchState());
        assertRefusedEverywhere("Ready", bytes);

        // The refusal took nothing from the draft: it still commits normally.
        final int bodiesBefore = bodyCount();
        final int undoBefore = undoDepth();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final android.widget.EditText field =
                    workspace.sketchEditor().findViewById(R.id.field_extrude_depth);
            field.setText("0.5");
            workspace.findViewById(R.id.sketch_extrude_commit).performClick();
            return null;
        });
        settleLayout();
        assertEquals("the same draft still commits", NativeViewport.SKETCH_INACTIVE, sketchState());
        assertEquals("as one new body", bodiesBefore + 1, bodyCount());
        assertEquals("and one Construction step", undoBefore + 1, undoDepth());

        assertLegalImport(file);
    }

    // -----------------------------------------------------------------------
    // IMPORT-SK-03: a STAGED Edit Sketch over a committed CAD body.
    // -----------------------------------------------------------------------

    @Test
    public void sk04_aStagedEditSketchRefusesImportAndStillCancelsCleanly() {
        final File file = writeFixture();
        final byte[] bytes = readFile(file);
        assertEquals(NativeViewport.CAD_OK, NativeViewport.sketchBegin(NativeViewport.WORKPLANE_XY));
        onNativeStateChanged(rule.getScenario());
        final long body = drawRectangleAndExtrude(rule.getScenario(), 1.0, 1.0, "0.5");
        final byte[] committed = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.encodeProject());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.CAD_OK, NativeViewport.sketchBeginEdit(body));
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        assertEquals("the staged edit is a live sketch", NativeViewport.SKETCH_EDITING,
                sketchState());
        assertRefusedEverywhere("staged Edit Sketch", bytes);

        cancelSketch();
        assertEquals("Cancel still leaves the committed body exactly as it was",
                describeBytes(committed), describeBytes(onWorkspace(rule.getScenario(),
                        (activity, workspace) -> NativeViewport.encodeProject())));
        assertLegalImport(file);
    }

    // -----------------------------------------------------------------------
    // IMPSK-UI-03: portrait and landscape, font scale 1.0 and 1.5.
    // -----------------------------------------------------------------------

    @Test
    public void sk05_theRowStaysAbsentAcrossOrientationAndFontScale() {
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
                    assertTrue(where + ": offered with no sketch", importOfferedByRealTap());
                    doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                        assertEquals(NativeViewport.CAD_OK,
                                NativeViewport.sketchBegin(NativeViewport.WORKPLANE_XY));
                        workspace.onNativeStateChanged();
                        return null;
                    });
                    settleLayout();
                    rig.touchId(R.id.project_actions_button);
                    final boolean offered = rig.shown(R.id.import_glb);
                    rig.capture("sk05_font_" + scale.replace('.', '_') + "_" + orientation);
                    closeProjectSurface();
                    assertFalse(where + ": not offered while sketching", offered);
                    cancelSketch();
                }
            }
        } finally {
            setFontScale("1.0");
        }
    }

    // -----------------------------------------------------------------------
    // Shared steps
    // -----------------------------------------------------------------------

    /** The bytes seam and the JNI entry, three times each: refused, neutral. */
    private void assertRefusedEverywhere(String where, byte[] bytes) {
        final Snapshot before = snapshot();
        for (int attempt = 0; attempt < 3; attempt++) {
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                workspace.applyImportedGlbBytes(bytes);
                return null;
            });
            settleLayout();
        }
        final Snapshot afterBytes = snapshot();
        final int[] statuses = new int[3];
        for (int attempt = 0; attempt < 3; attempt++) {
            statuses[attempt] = onWorkspace(rule.getScenario(),
                    (activity, workspace) -> NativeViewport.importGlbDurable(bytes));
        }
        final String token = onWorkspace(rule.getScenario(),
                (activity, workspace) -> statuses[0] >= NativeViewport.IMPORT_COMMIT_BASE
                        ? NativeViewport.glbCommitStatusToken(statuses[0])
                        : NativeViewport.glbImportStatusToken(statuses[0]));
        settleLayout();
        final Snapshot afterJni = snapshot();
        Log.i(TAG, "IMPSK_OBSERVED_NATIVE where=" + where + " statuses="
                + Arrays.toString(statuses) + " token=" + token + " before=" + before
                + " afterBytes=" + afterBytes + " afterJni=" + afterJni);
        rig.fact("native_" + where.replace(' ', '_'), Arrays.toString(statuses) + " " + token);

        assertEquals(where + ": the bytes entry changes nothing", before.toString(),
                afterBytes.toString());
        for (int status : statuses) {
            assertEquals(where + ": the native import refuses with the appended project code",
                    REFUSED_IN_SKETCH, status);
        }
        assertEquals(where + ": by name", "RefusedInSketch", token);
        assertEquals(where + ": and the direct JNI calls change nothing either",
                before.toString(), afterJni.toString());
    }

    /** The SAME file, through the picker result, once no sketch is open. */
    private void assertLegalImport(File file) {
        assertEquals("precondition: no sketch", NativeViewport.SKETCH_INACTIVE, sketchState());
        final long[] idsBefore = sceneBodyIds();
        final int undoBefore = undoDepth();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onOpenGlbDocumentChosen(Uri.fromFile(file));
            return null;
        });
        settleLayout();
        final long[] idsAfter = sceneBodyIds();
        assertEquals("the same file imports its six objects with no sketch open",
                idsBefore.length + SIX_NODE_OBJECTS, idsAfter.length);
        assertEquals("the bodies that were there are untouched and first",
                Arrays.toString(idsBefore),
                Arrays.toString(Arrays.copyOf(idsAfter, idsBefore.length)));
        // That a refusal minted no id is proved by the snapshot: the encoded
        // project carries the allocator's high-water mark. Here, only that the
        // new ids are fresh and one contiguous range, the first of them active.
        for (int i = idsBefore.length + 1; i < idsAfter.length; i++) {
            assertEquals("the import's own ids are one contiguous range",
                    idsAfter[i - 1] + 1, idsAfter[i]);
        }
        for (long existing : idsBefore) {
            assertTrue("every new id is fresh", idsAfter[idsBefore.length] > existing);
        }
        assertEquals("the first imported body is active", idsAfter[idsBefore.length],
                (long) onWorkspace(rule.getScenario(),
                        (activity, workspace) -> NativeViewport.sceneActiveBodyId()));
        assertEquals("the whole import is one Construction step", undoBefore + 1, undoDepth());
        // It survives save / reopen like any imported body.
        final byte[] saved = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.encodeProject());
        assertEquals(NativeViewport.PROJECT_OK, (int) onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.loadProject(saved)));
        assertEquals("every body reopens", idsAfter.length, sceneBodyIds().length);
    }

    private void beginSketchWithARectangle() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.CAD_OK,
                    NativeViewport.sketchBegin(NativeViewport.WORKPLANE_XY));
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), -0.5, -0.5, 0.5, 0.5);
        assertEquals("one rectangle in the draft", 1, sketchEntityCount());
    }

    private void finishToReady() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.finish_sketch).performClick();
            return null;
        });
        settleLayout();
        assertEquals(NativeViewport.SKETCH_READY, sketchState());
    }

    private void cancelSketch() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.sketchCancel();
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        assertEquals(NativeViewport.SKETCH_INACTIVE, sketchState());
    }

    /** Opens the project drawer by a REAL tap on the mark; reads the row; closes. */
    private boolean importOfferedByRealTap() {
        rig.touchId(R.id.project_actions_button);
        final boolean offered = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View row = workspace.findViewById(R.id.import_glb);
            assertNotNull("the import row exists in the view tree", row);
            return workspace.projectPopover().isOpen() && row.isShown() && row.isEnabled();
        });
        closeProjectSurface();
        return offered;
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

    /** Records picker requests instead of launching the system UI. */
    private static final class RecordingTransferHost
            implements EditorWorkspaceView.ProjectTransferHost {
        private final int[] glbRequests;

        RecordingTransferHost(int[] glbRequests) {
            this.glbRequests = glbRequests;
        }

        @Override
        public boolean requestCreateProjectDocument() {
            return false;
        }

        @Override
        public boolean requestOpenProjectDocument() {
            return false;
        }

        @Override
        public boolean requestCreateDiagnosticsDocument() {
            return false;
        }

        @Override
        public boolean requestCreateGlbDocument() {
            return false;
        }

        @Override
        public boolean requestOpenGlbDocument() {
            glbRequests[0]++;
            return true;
        }
    }

    // -----------------------------------------------------------------------
    // The neutrality snapshot
    // -----------------------------------------------------------------------

    /** Everything an import would move, and the draft it must leave alone. */
    private static final class Snapshot {
        int mode;
        long activeBodyId;
        String bodyIds;
        int undo;
        int redo;
        long fingerprint;
        String projectDigest;
        int sketchState;
        int sketchEntities;

        @Override
        public String toString() {
            return "{mode=" + mode + " active=" + activeBodyId + " ids=" + bodyIds
                    + " undo=" + undo + " redo=" + redo
                    + " fingerprint=" + Long.toHexString(fingerprint)
                    + " project=" + projectDigest + " sketchState=" + sketchState
                    + " sketchEntities=" + sketchEntities + "}";
        }
    }

    private Snapshot snapshot() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final Snapshot s = new Snapshot();
            s.mode = NativeViewport.productMode();
            s.activeBodyId = NativeViewport.sceneActiveBodyId();
            final long[] ids = new long[NativeViewport.sceneBodyCount()];
            NativeViewport.sceneBodyIds(ids);
            s.bodyIds = Arrays.toString(ids);
            s.undo = NativeViewport.constructionUndoDepth();
            s.redo = NativeViewport.constructionRedoDepth();
            s.fingerprint = NativeViewport.projectFingerprint();
            s.projectDigest = describeBytes(NativeViewport.encodeProject());
            final double[] sketch = new double[NativeViewport.SKETCH_STATE_SIZE];
            NativeViewport.sketchState(sketch);
            s.sketchState = (int) sketch[NativeViewport.SKETCH_STATE];
            s.sketchEntities = (int) sketch[NativeViewport.SKETCH_ENTITY_COUNT];
            return s;
        });
    }

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    private int bodyCount() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneBodyCount());
    }

    private int undoDepth() {
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
