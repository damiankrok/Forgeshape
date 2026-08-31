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

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.charset.StandardCharsets;

/**
 * `FSR1B-10..13`: a ForgeShape project can be moved through the device's own
 * storage, and nothing about that storage can get into the project.
 *
 * <h2>What the seam is, and what it is not</h2>
 *
 * <p>These cases drive the <b>production</b> handlers —
 * {@code onCreateProjectDocumentChosen} and {@code onOpenProjectDocumentChosen}
 * — with a real {@code Uri} that a real {@code ContentResolver} opens. What they
 * do not do is tap through the system's document picker UI, which is another
 * app's surface, differs per device, and cannot be driven reliably from
 * instrumentation.
 *
 * <p>So the split is: what happens to the bytes is tested for real, end to end,
 * against a real resolver; and what is ASKED of the system is tested by
 * asserting the exact Intent that would be sent. Between them that covers
 * everything ForgeShape is responsible for. It is stated plainly rather than
 * implied, because "SAF works" would be a bigger claim than these cases make.
 */
@RunWith(AndroidJUnit4.class)
public final class ProjectTransferTest {

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private final File scratch = new File(context().getCacheDir(), "transfer-test");

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
    // FSR1B-10: Save Copy writes bytes the native codec accepts
    // -----------------------------------------------------------------------

    @Test
    public void fsr1b10_saveCopyWritesADocumentTheNativeCodecAccepts() {
        edit(4.5, 2.25, 1.125);
        final byte[] expected = encodeProject();

        final File destination = new File(scratch, "exported.forge");
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onSaveCopyRequestedForTest(expected);
            workspace.onCreateProjectDocumentChosen(Uri.fromFile(destination));
            return null;
        });
        settleLayout();

        assertTrue("the document exists where the user chose", destination.isFile());
        final byte[] written = readFile(destination);
        assertArrayEquals("and holds exactly the canonical project bytes", expected, written);
        assertEquals("which the real decoder accepts", NativeViewport.PROJECT_OK,
                NativeViewport.validateProject(written));
    }

    @Test
    public void fsr1b10_saveCopyTruncatesAnExistingDocumentRatherThanOverwritingItsFront() {
        edit(3.0, 1.5, 0.75);
        final byte[] expected = encodeProject();

        // A file longer than the project. Overwriting only its front would leave
        // a valid `.forge` header in front of somebody else's trailing bytes,
        // which decodes as a damaged project rather than as the copy asked for.
        final File destination = new File(scratch, "preexisting.forge");
        final byte[] filler = new byte[expected.length + 4096];
        for (int i = 0; i < filler.length; i++) {
            filler[i] = (byte) (i % 251);
        }
        writeFile(destination, filler);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onSaveCopyRequestedForTest(expected);
            workspace.onCreateProjectDocumentChosen(Uri.fromFile(destination));
            return null;
        });
        settleLayout();

        assertEquals("the document is exactly the project's length, not longer",
                expected.length, destination.length());
        assertArrayEquals(expected, readFile(destination));
    }

    // -----------------------------------------------------------------------
    // FSR1B-11: Open File validates, then applies atomically
    // -----------------------------------------------------------------------

    @Test
    public void fsr1b11_openFileAppliesAValidDocument() {
        edit(6.25, 3.0, 1.5);
        final byte[] source = encodeProject();
        final File document = new File(scratch, "incoming.forge");
        writeFile(document, source);

        // Move the live project well away, so "the values came back" cannot be
        // true by accident.
        edit(1.5, 1.0, 0.25);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onOpenProjectDocumentChosen(Uri.fromFile(document));
            return null;
        });
        settleLayout();

        final double[] primitive = primitiveState();
        assertEquals(6.25, primitive[NativeViewport.PRIMITIVE_BOX_WIDTH], 0.0);
        assertEquals(3.0, primitive[NativeViewport.PRIMITIVE_BOX_WIDTH + 1], 0.0);
        assertEquals(1.5, primitive[NativeViewport.PRIMITIVE_BOX_WIDTH + 2], 0.0);
        assertEquals("a load starts a fresh session history", 0,
                (int) onWorkspace(rule.getScenario(),
                        (activity, workspace) -> NativeViewport.constructionUndoDepth()));
    }

    @Test
    public void fsr1b11_openFileRefusesADamagedDocumentAndChangesNothing() {
        edit(5.5, 2.75, 1.375);
        final byte[] good = encodeProject();
        final byte[] damaged = good.clone();
        damaged[28 + 24 + 5] ^= 0x01;  // one bit inside the SCNE payload
        final File document = new File(scratch, "damaged.forge");
        writeFile(document, damaged);

        final double[] before = WorkspaceTestSupport.nativeSnapshot();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onOpenProjectDocumentChosen(Uri.fromFile(document));
            return null;
        });
        settleLayout();

        assertArrayEquals("a refused open moves nothing below JNI",
                before, WorkspaceTestSupport.nativeSnapshot(), 0.0);
    }

    @Test
    public void fsr1b11_anUnreadableDocumentIsNonDestructive() {
        edit(2.75, 1.25, 0.5);
        final double[] before = WorkspaceTestSupport.nativeSnapshot();

        // A Uri that names nothing. A provider that has gone away, a permission
        // that was revoked and a file that was deleted between the pick and the
        // read all arrive here.
        final Uri missing = Uri.fromFile(new File(scratch, "does-not-exist.forge"));
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onOpenProjectDocumentChosen(missing);
            return null;
        });
        settleLayout();

        assertArrayEquals("nothing moved", before, WorkspaceTestSupport.nativeSnapshot(), 0.0);
        assertNull("and the reader reported it rather than throwing",
                ProjectTransfer.readFrom(context(), missing));
    }

    @Test
    public void fsr1b11_cancellingEitherPickerIsANoOp() {
        edit(3.75, 1.75, 0.875);
        final double[] before = WorkspaceTestSupport.nativeSnapshot();
        final byte[] pending = encodeProject();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            // A cancel arrives as a null Uri through the same handler, so
            // "the user backed out" is one honest no-op rather than a silently
            // different path.
            workspace.onSaveCopyRequestedForTest(pending);
            workspace.onCreateProjectDocumentChosen(null);
            workspace.onOpenProjectDocumentChosen(null);
            workspace.onCreateDiagnosticsDocumentChosen(null);
            return null;
        });
        settleLayout();

        assertArrayEquals("a cancel changes nothing",
                before, WorkspaceTestSupport.nativeSnapshot(), 0.0);
        assertEquals("and writes no file to the scratch area", 0,
                scratch.listFiles() == null ? 0 : scratch.listFiles().length);

        // And the bytes it was holding are released, so a later pick for a
        // different action cannot write the project the user cancelled.
        final File destination = new File(scratch, "after-cancel.forge");
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onCreateProjectDocumentChosen(Uri.fromFile(destination));
            return null;
        });
        settleLayout();
        assertFalse("a cancelled copy cannot be written by a later pick",
                destination.isFile());
    }

    // -----------------------------------------------------------------------
    // FSR1B-12: Open File does not touch the internal manual slot
    // -----------------------------------------------------------------------

    @Test
    public void fsr1b12_openingAFileDoesNotReplaceTheInternalSavedProject() {
        // The user's own saved project.
        edit(2.0, 1.0, 0.5);
        saveThroughTheProductControl();
        final byte[] savedProject = ProjectSlot.read(context());
        assertNotNull("precondition: an explicit save exists", savedProject);

        // A different project, in a file.
        edit(7.5, 3.75, 1.875);
        final byte[] fromFile = encodeProject();
        final File document = new File(scratch, "other.forge");
        writeFile(document, fromFile);
        edit(2.0, 1.0, 0.5);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onOpenProjectDocumentChosen(Uri.fromFile(document));
            return null;
        });
        settleLayout();

        assertEquals("the opened project is live", 7.5,
                primitiveState()[NativeViewport.PRIMITIVE_BOX_WIDTH], 0.0);
        assertArrayEquals("and the internal saved project is byte-identical",
                savedProject, ProjectSlot.read(context()));
    }

    @Test
    public void fsr1b12_savingACopyDoesNotTouchTheInternalSavedProject() {
        edit(2.0, 1.0, 0.5);
        saveThroughTheProductControl();
        final byte[] savedProject = ProjectSlot.read(context());

        edit(8.0, 4.0, 2.0);
        final byte[] copy = encodeProject();
        final File destination = new File(scratch, "copy.forge");
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onSaveCopyRequestedForTest(copy);
            workspace.onCreateProjectDocumentChosen(Uri.fromFile(destination));
            return null;
        });
        settleLayout();

        assertTrue(destination.isFile());
        assertArrayEquals("Save Copy leaves the internal slot alone",
                savedProject, ProjectSlot.read(context()));
    }

    // -----------------------------------------------------------------------
    // FSR1B-13: nothing device-local reaches the document
    // -----------------------------------------------------------------------

    @Test
    public void fsr1b13_noUriOrPathSurvivesIntoTheProjectDocument() {
        edit(9.25, 4.5, 2.25);
        final byte[] original = encodeProject();

        // A deliberately distinctive filename. If ANY part of where a project
        // came from leaked into the document, this is what it would look like.
        final File document = new File(scratch, "zz-provenance-marker-4242.forge");
        writeFile(document, original);

        edit(1.0, 1.0, 1.0);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onOpenProjectDocumentChosen(Uri.fromFile(document));
            return null;
        });
        settleLayout();

        // Re-encoding the project that came from that file must reproduce the
        // original bytes EXACTLY. A document that remembered its origin — a
        // path, a Uri, an authority, a timestamp — could not possibly do that.
        final byte[] reencoded = encodeProject();
        assertArrayEquals("a project carries no trace of where it came from",
                original, reencoded);
        assertFalse("and its bytes contain no filename",
                containsAscii(reencoded, "provenance-marker"));
        assertFalse("no scheme", containsAscii(reencoded, "file:"));
        assertFalse("no content authority", containsAscii(reencoded, "content:"));
        assertFalse("and no filesystem path", containsAscii(reencoded, "/data/"));
    }

    @Test
    public void fsr1b13_theCodecNeverSeesAUri() {
        // Structural, and the strongest form of the claim available in Java:
        // the two native entry points that carry a project take a byte array and
        // nothing else. A Uri could not reach the codec if someone tried.
        for (String method : new String[]{"loadProject", "validateProject"}) {
            boolean found = false;
            for (java.lang.reflect.Method declared : NativeViewport.class.getDeclaredMethods()) {
                if (!declared.getName().equals(method)) {
                    continue;
                }
                found = true;
                final Class<?>[] parameters = declared.getParameterTypes();
                assertEquals(method + " takes exactly one argument", 1, parameters.length);
                assertEquals(method + " takes bytes, never a Uri or a path",
                        byte[].class, parameters[0]);
            }
            assertTrue(method + " must exist", found);
        }
        for (java.lang.reflect.Method declared : NativeViewport.class.getDeclaredMethods()) {
            if (declared.getName().equals("encodeProject")) {
                assertEquals("encodeProject takes nothing", 0,
                        declared.getParameterTypes().length);
                assertEquals("and returns bytes", byte[].class, declared.getReturnType());
            }
        }
    }

    // -----------------------------------------------------------------------
    // What is asked of the system
    // -----------------------------------------------------------------------

    @Test
    public void fsr1b10_theCreateIntentAsksForADocumentNamedForge() {
        final Intent intent = ProjectTransfer.createDocumentIntent();
        assertEquals(Intent.ACTION_CREATE_DOCUMENT, intent.getAction());
        assertTrue(intent.getCategories().contains(Intent.CATEGORY_OPENABLE));
        assertEquals(ProjectTransfer.CREATE_MIME_TYPE, intent.getType());
        final String title = intent.getStringExtra(Intent.EXTRA_TITLE);
        assertNotNull(title);
        assertTrue("the product's own extension stays user-visible",
                title.endsWith(".forge"));
    }

    @Test
    public void fsr1b11_theOpenIntentAsksForADocumentToRead() {
        final Intent intent = ProjectTransfer.openDocumentIntent();
        assertEquals(Intent.ACTION_OPEN_DOCUMENT, intent.getAction());
        assertTrue(intent.getCategories().contains(Intent.CATEGORY_OPENABLE));
        // Deliberately permissive: a `.forge` file has no registered type, and
        // filtering narrowly would hide the user's own project from the picker.
        // The fail-closed decoder is the actual guard.
        assertEquals(ProjectTransfer.OPEN_MIME_TYPE, intent.getType());
    }

    // -----------------------------------------------------------------------
    // FSR1B-18: no standard interchange format was added
    // -----------------------------------------------------------------------

    @Test
    public void fsr1b18_noInterchangeFormatIsOfferedAnywhereInTheProjectSurface() {
        openProjectSurface();
        // OBJ, FBX and the rest are absent in both directions and always were.
        // GLB is deliberately NOT on this list any more: GLB-IMPORT-R0 added a
        // named DIAGNOSTIC that reads a `.glb` back to check it, under
        // ARCH-OWNER-08. What that changed is what the surface offers, not what
        // the product supports, so the rest of this case tightened rather than
        // relaxed — see below.
        final String[] forbidden = {"gltf", "obj", "fbx", "stl", "collada", "dae", "usdz"};
        final StringBuilder visible = new StringBuilder();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            collectText(workspace.projectPopover(), visible);
            return null;
        });
        final String text = visible.toString().toLowerCase(java.util.Locale.US);
        for (String word : forbidden) {
            assertFalse("the project surface must not offer '" + word + "': " + text,
                    text.contains(word));
        }

        // The `.forge` transfer group still says nothing about a format or an
        // export: Save Copy… and Open File… move ForgeShape projects, and
        // wording that blurred that is exactly what this case was written for.
        final String transferWording = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final StringBuilder rows = new StringBuilder();
            collectText(workspace.findViewById(R.id.project_save_copy), rows);
            collectText(workspace.findViewById(R.id.project_open_file), rows);
            collectText(workspace.findViewById(R.id.project_save), rows);
            collectText(workspace.findViewById(R.id.project_open), rows);
            return rows.toString().toLowerCase(java.util.Locale.US);
        });
        for (String word : new String[]{"glb", "gltf", "export", "import"}) {
            assertFalse("a `.forge` transfer row must not mention '" + word + "': "
                    + transferWording, transferWording.contains(word));
        }

        // And the GLB group names the act — Import GLB — while every word
        // around it says PREVIEW, which is what the user gets (GLB-IMPORT-R1,
        // `ARCH-OWNER-09`). The `.forge` rows above still say neither, which
        // is what keeps the two acts apart: Open File opens a project, Import
        // GLB previews somebody else's mesh.
        assertTrue("the GLB row must name the act: " + text, text.contains("import glb"));
        assertTrue("and it must be framed as a preview, not as production import: " + text,
                text.contains("preview"));

        // Export exists and works, but it lives in the Global Toolbar, not
        // here: writing a `.glb` is a different act with a different
        // destination from moving a project.
        assertNotNull("Export keeps its own home in the toolbar",
                onWorkspace(rule.getScenario(),
                        (activity, workspace) -> workspace.findViewById(R.id.export_action)));
    }

    // -----------------------------------------------------------------------
    // Shared machinery
    // -----------------------------------------------------------------------

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

    private double[] primitiveState() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] state = new double[NativeViewport.PRIMITIVE_STATE_SIZE];
            NativeViewport.constructionPrimitive(state);
            return state;
        });
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

    private void saveThroughTheProductControl() {
        openProjectSurface();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.project_save).performClick();
            return null;
        });
        settleLayout();
    }

    private static void collectText(android.view.View view, StringBuilder out) {
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

    private static boolean containsAscii(byte[] haystack, String needle) {
        final byte[] pattern = needle.getBytes(StandardCharsets.US_ASCII);
        outer:
        for (int i = 0; i + pattern.length <= haystack.length; i++) {
            for (int j = 0; j < pattern.length; j++) {
                if (haystack[i + j] != pattern[j]) {
                    continue outer;
                }
            }
            return true;
        }
        return false;
    }

    private static void writeFile(File file, byte[] bytes) {
        try (FileOutputStream out = new FileOutputStream(file)) {
            out.write(bytes);
        } catch (IOException error) {
            throw new AssertionError("the scratch file must be writable", error);
        }
    }

    private static byte[] readFile(File file) {
        try (RandomAccessFile in = new RandomAccessFile(file, "r")) {
            final byte[] bytes = new byte[(int) in.length()];
            in.readFully(bytes);
            return bytes;
        } catch (IOException error) {
            throw new AssertionError("the scratch file must be readable", error);
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
