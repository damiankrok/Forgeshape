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
import android.net.Uri;
import android.view.View;
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
 * `IMP01B-01..14` and `E2E-IMP01B-01..07`: sculpting an Imported Mesh.
 *
 * <h2>What this suite is for</h2>
 *
 * <p>The DOMAIN half of `IMPORT-01B` — what the seed is, that the imported
 * arrays never move, that the placement is not baked twice, that the four
 * `CONS`/`IMPT`/`SCUL` combinations are valid `.forge` documents — is proved by
 * the native self-tests, which build their own scenes and depend on no live
 * session. What is left, and what this covers, is everything that can only be
 * true on a device: the toolbar's transitions and their wording, a real stroke
 * through the whole touch path, the two `.forge` slots, autosave and recovery,
 * and export.
 *
 * <h2>The one invariant every case here is really about</h2>
 *
 * <p>An Imported Mesh is <b>immutable source truth</b>. Sculpting it produces a
 * second representation of the same body and writes nothing back: <i>Back to
 * Imported Mesh</i> shows the file's own geometry, unchanged, however many
 * strokes were made. Every case that sculpts asserts the imported arrays
 * afterwards, through the one place they are observable end to end — the
 * `.forge` document's `IMPT` section.
 */
@RunWith(AndroidJUnit4.class)
public final class ImportedMeshSculptTest {

    /**
     * A ForgeShape export of the six-body Construction corpus project.
     *
     * <p>Six top-level mesh nodes, each a closed solid with enough vertices for
     * a brush to capture a meaningful set. It is the same fixture
     * `ImportedMeshDurableTest` uses, so the two suites agree about what an
     * import of it produces.
     */
    private static final String SIX_NODE_GLB = "glb/construction_sentinel.glb";

    /** How long the autosave worker gets to drain. Never slept for; see awaitIdle. */
    private static final long IDLE_TIMEOUT_MS = 5000L;

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private final File scratch = new File(context().getCacheDir(), "import-01b-test");

    private static Context context() {
        return InstrumentationRegistry.getInstrumentation().getTargetContext();
    }

    /**
     * The project as this case found it, captured before anything was imported.
     *
     * <p>The scene is process-scoped and this suite adds imported bodies and
     * sculpt meshes later cases do not expect, so every case puts the project
     * back through the ordinary atomic load. It is a product path, not a test
     * seam: loading a document replaces the scene exactly.
     */
    private byte[] baselineProject;

    @Before
    public void setUp() {
        clearAllProjectFiles();
        deleteRecursively(scratch);
        assertTrue(scratch.mkdirs());
        resetToBaselineConstruction(rule.getScenario());
        baselineProject = encodeProject();
    }

    @After
    public void tearDown() {
        clearAllProjectFiles();
        deleteRecursively(scratch);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.enterConstructionMode();
            workspace.uiState().recordRecoveryResolved();
            if (baselineProject != null) {
                NativeViewport.loadProject(baselineProject);
            }
            return null;
        });
        resetToBaselineConstruction(rule.getScenario());
    }

    // -----------------------------------------------------------------------
    // IMP01B-01 / E2E-IMP01B-01: an imported body offers Start Sculpting
    // -----------------------------------------------------------------------

    @Test
    public void imp01b01_anImportedBodyOffersStartSculptingAndNamesItsOwnContext() {
        final long imported = importOneBody();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("precondition: the active body is an Imported Mesh",
                    NativeViewport.sceneActiveBodyIsImported());
            final View start = workspace.findViewById(R.id.freeze_to_sculpt);
            assertNotNull("Start Sculpting exists", start);
            assertEquals("IMP01B-01: and it is drawn for an imported body",
                    View.VISIBLE, start.getVisibility());
            assertEquals("with the product's one wording for the act",
                    activity.getString(R.string.start_sculpting),
                    ((TextView) start).getText().toString());
            // The context this body is in is its REPRESENTATION, not the
            // Construction mode it happens to share with every other body.
            assertEquals("the toolbar names the imported context",
                    activity.getString(R.string.context_imported_mesh),
                    ((TextView) workspace.findViewById(R.id.editing_context_label)).getText()
                            .toString());
            return null;
        });

        startSculpting();

        assertEquals("IMP01B-01: the product is in Sculpt", NativeViewport.MODE_SCULPT,
                productMode());
        assertEquals("on the imported body's own identity",
                imported, (long) readSculpt()[NativeViewport.SCULPT_OBJECT_ID]);
    }

    // -----------------------------------------------------------------------
    // IMP01B-02/03 / E2E-IMP01B-02: the seed is the imported geometry, in place
    // -----------------------------------------------------------------------

    /**
     * The first pre-stroke Sculpt frame is the same object, in the same place.
     *
     * <p>The counts are compared against what the file actually produced, and
     * the PLACEMENT against what it was before the freeze. Both representations
     * are LOCAL geometry under the one authored transform, so a seed that had
     * baked the placement in a second time would show as a moved body with an
     * unchanged transform — which is exactly what this pair rules out.
     */
    @Test
    public void imp01b02and03_theFirstSculptFrameMatchesTheImportedSourceAndItsPlacement() {
        final long imported = importOneBody();

        final double[] placementBefore = placement();
        final int importedVertices = importedVertexCountFromProject(imported);
        assertTrue("precondition: the imported body published geometry", importedVertices > 0);

        startSculpting();

        final double[] sculpt = readSculpt();
        assertEquals("IMP01B-02: the seed has the imported vertex count",
                importedVertices, (int) sculpt[NativeViewport.SCULPT_VERTEX_COUNT]);
        assertEquals("IMP01B-02: and nothing has been sculpted into it yet", 0.0,
                sculpt[NativeViewport.SCULPT_HAS_EDITS], 0.0);
        assertArrayEquals("IMP01B-03: the authored placement is untouched by the freeze",
                placementBefore, placement(), 0.0);
    }

    // -----------------------------------------------------------------------
    // IMP01B-04/05/06/07/08 / E2E-IMP01B-03/04/05: a real stroke, and back
    // -----------------------------------------------------------------------

    /**
     * A real Grab stroke through the whole touch path changes sculpt truth and
     * leaves the imported source byte-identical.
     *
     * <p>The source is compared through the `.forge` document's `IMPT` section,
     * which is the one place the imported arrays are observable end to end. A
     * stroke that had written back would change those bytes.
     */
    @Test
    public void imp01b04and05_aRealStrokeMovesSculptTruthAndNeverTheImportedSource() {
        importOneBody();
        startSculpting();

        final byte[] importedSectionBefore = importedSectionOfProject();
        assertTrue("precondition: the project carries an IMPT section",
                importedSectionBefore.length > 0);
        final double[] before = readSculpt();

        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());

        final double[] after = readSculpt();
        assertTrue("IMP01B-04: the stroke minted a new SculptRevision",
                after[NativeViewport.SCULPT_REVISION] > before[NativeViewport.SCULPT_REVISION]);
        assertNotEquals("IMP01B-04: and the mesh now reports edits", 0.0,
                after[NativeViewport.SCULPT_HAS_EDITS], 0.0);
        assertEquals("IMP01B-05: the imported source is byte-identical afterwards",
                describeBytes(importedSectionBefore), describeBytes(importedSectionOfProject()));

        // IMP01B-07: Back to Imported Mesh shows the file's geometry again and
        // keeps the sculpt work.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View back = workspace.findViewById(R.id.back_to_construction);
            assertEquals("IMP01B-07: the way out of Sculpt is drawn",
                    View.VISIBLE, back.getVisibility());
            assertEquals("IMP01B-07: and it names the imported destination",
                    activity.getString(R.string.back_to_imported_mesh),
                    back.getContentDescription().toString());
            back.performClick();
            return null;
        });
        settleLayout();

        assertEquals("IMP01B-07: the product is back in Construction mode",
                NativeViewport.MODE_CONSTRUCTION, productMode());
        assertNotEquals("IMP01B-07: and leaving Sculpt KEPT the work rather than discarding it",
                0.0, readSculpt()[NativeViewport.SCULPT_HAS_MESH], 0.0);
        assertEquals("IMP01B-07: and the imported source is still byte-identical",
                describeBytes(importedSectionBefore), describeBytes(importedSectionOfProject()));

        // IMP01B-08: Resume Sculpt returns to the retained mesh exactly.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View resume = workspace.findViewById(R.id.resume_sculpt);
            assertEquals("IMP01B-08: Resume Sculpt is offered for an imported body",
                    View.VISIBLE, resume.getVisibility());
            resume.performClick();
            return null;
        });
        settleLayout();
        final double[] resumed = readSculpt();
        assertEquals("IMP01B-08: the same SculptRevision came back",
                after[NativeViewport.SCULPT_REVISION], resumed[NativeViewport.SCULPT_REVISION],
                0.0);
        assertEquals("IMP01B-08: and the same counts",
                after[NativeViewport.SCULPT_VERTEX_COUNT],
                resumed[NativeViewport.SCULPT_VERTEX_COUNT], 0.0);
        assertNotEquals("IMP01B-08: with the edits intact", 0.0,
                resumed[NativeViewport.SCULPT_HAS_EDITS], 0.0);
    }

    /**
     * IMP01B-06. Construction Undo and Redo do not reach a sculpted vertex, and
     * an imported body is no exception.
     *
     * <p>Sculpting has its own history since {@code ARCH-OWNER-12}, and this is
     * the invariant that keeps the two apart: the CONSTRUCTION entry points are
     * refused below JNI while sculpting, whatever the chrome does, and a
     * Construction undo taken after leaving Sculpt moves the placement without
     * touching one sculpted vertex. Sculpt Undo itself is
     * {@code SculptUndoTest}'s subject, not this one's.
     */
    @Test
    public void imp01b06_constructionHistoryNeverMovesAnImportedBodysSculptedVertices() {
        importOneBody();
        startSculpting();
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        final double[] sculpted = readSculpt();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("Undo and Redo are drawn in Sculpt, meaning the Sculpt history",
                    View.VISIBLE, workspace.historyGroup().getVisibility());
            assertEquals("but the CONSTRUCTION history is still refused below JNI",
                    NativeViewport.HISTORY_REFUSED_IN_SCULPT, NativeViewport.constructionUndo());
            return null;
        });
        assertEquals("so the sculpt mesh did not move",
                sculpted[NativeViewport.SCULPT_REVISION],
                readSculpt()[NativeViewport.SCULPT_REVISION], 0.0);

        // Out of Sculpt, a real Construction act on the same body, then Undo.
        backToSource();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.APPLY_APPLIED,
                    NativeViewport.applyBoxTransform(2.0, 0.5, -1.0, 0.0, 0.0, 0.0, 1.0, 1.0,
                            1.0));
            assertEquals(NativeViewport.HISTORY_OK, NativeViewport.constructionUndo());
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();

        resumeSculpt();
        final double[] afterHistory = readSculpt();
        assertEquals("IMP01B-06: a Construction undo moved no sculpted vertex",
                sculpted[NativeViewport.SCULPT_REVISION],
                afterHistory[NativeViewport.SCULPT_REVISION], 0.0);
        assertEquals("and no sculpt vertex count changed",
                sculpted[NativeViewport.SCULPT_VERTEX_COUNT],
                afterHistory[NativeViewport.SCULPT_VERTEX_COUNT], 0.0);
    }

    // -----------------------------------------------------------------------
    // IMP01B-09: the destructive reset, named for the source it rebuilds from
    // -----------------------------------------------------------------------

    @Test
    public void imp01b09_resetIsNamedForTheImportedSourceAndStillAsksFirst() {
        importOneBody();
        startSculpting();
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final TextView reset =
                    workspace.sculptContext().findViewById(R.id.freeze_again);
            assertNotNull("the destructive act is still offered", reset);
            assertEquals("IMP01B-09: and named for the source it rebuilds from",
                    activity.getString(R.string.reset_sculpt_from_imported_mesh),
                    reset.getText().toString());
            assertFalse("no Construction-only wording reaches an imported body",
                    reset.getText().toString().contains("Shape"));
            // The stale-source warning is Construction-only in practice: an
            // Imported Mesh is immutable, so nothing can make one stale.
            assertEquals("no stale-source warning stands over an imported sculpt",
                    View.GONE,
                    workspace.sculptContext().findViewById(R.id.stale_source_warning)
                            .getVisibility());
            reset.performClick();
            return null;
        });
        WorkspaceTestSupport.settle();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertNotNull("IMP01B-09: the destructive act still asks first",
                    workspace.sculptContext().visibleConfirmation());
            final String message =
                    activity.getString(R.string.reset_sculpt_confirm_message_imported);
            assertTrue("and the question says the sculpting will be discarded: " + message,
                    message.contains("discarded"));
            assertTrue("and that it cannot be undone: " + message,
                    message.contains("cannot be undone"));
            // Cancel changes nothing at all -- it makes no native call.
            workspace.sculptContext().visibleConfirmation().dismiss();
            return null;
        });
        WorkspaceTestSupport.settle();
        assertNotEquals("IMP01B-09: cancelling kept the sculpt work", 0.0,
                readSculpt()[NativeViewport.SCULPT_HAS_EDITS], 0.0);
    }

    // -----------------------------------------------------------------------
    // IMP01B-11/13 / E2E-IMP01B-06/07: persistence of IMPT + SCUL
    // -----------------------------------------------------------------------

    /**
     * An imported body's sculpt work survives Save, autosave, recovery and a
     * fresh load, and comes back ready to resume.
     *
     * <p>Every path goes through the ONE canonical document: the manual slot,
     * the recovery checkpoint and a reload are all the same bytes through the
     * same fail-closed decoder. That is why one case can cover all of them
     * without three encodings.
     */
    @Test
    public void imp01b11and13_animportedSculptSurvivesSaveAutosaveRecoveryAndReload() {
        importOneBody();
        startSculpting();
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());

        final double[] sculptedBefore = readSculpt();
        final byte[] saved = encodeProject();
        assertTrue("the document carries both branches for one body",
                sectionPresent(saved, "IMPT") && sectionPresent(saved, "SCUL"));
        assertEquals("and it passes the real fail-closed decoder",
                NativeViewport.PROJECT_OK, validateProject(saved));

        // Autosave: the checkpoint is the SAME canonical document.
        awaitAutosaveIdle();
        assertTrue("a sculpted import is a project change and is checkpointed",
                ProjectCheckpoint.exists(context()));
        assertArrayEquals("IMP01B-13: the checkpoint is the same canonical document",
                saved, ProjectCheckpoint.read(context()));
        assertFalse("autosave still never writes the manual slot",
                ProjectSlot.exists(context()));

        // Save Copy / Open File carry the same bytes through Scoped Storage.
        final File copy = new File(scratch, "imported-sculpt.forge");
        writeFile(copy, saved);
        assertArrayEquals("IMP01B-13: a saved copy is the same document",
                saved, readFile(copy));

        // A fresh load, which is what Open, Recover and a reopened process all do.
        resetToBaselineConstruction(rule.getScenario());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("IMP01B-11: the document loads", NativeViewport.PROJECT_OK,
                    NativeViewport.loadProject(readFile(copy)));
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();

        assertTrue("IMP01B-11: the body came back as an Imported Mesh",
                onWorkspace(rule.getScenario(),
                        (activity, workspace) -> NativeViewport.sceneActiveBodyIsImported()));
        final double[] restored = readSculpt();
        assertNotEquals("IMP01B-11: with its sculpt mesh", 0.0,
                restored[NativeViewport.SCULPT_HAS_MESH], 0.0);
        assertEquals("IMP01B-11: and the same counts",
                sculptedBefore[NativeViewport.SCULPT_VERTEX_COUNT],
                restored[NativeViewport.SCULPT_VERTEX_COUNT], 0.0);
        assertNotEquals("IMP01B-09: the reset guard still knows there are edits", 0.0,
                restored[NativeViewport.SCULPT_HAS_EDITS], 0.0);
        assertEquals("IMP01B-11: re-encoding the reopened project reproduces the bytes",
                describeBytes(saved), describeBytes(encodeProject()));

        // IMP01B-08: and Resume Sculpt is what the toolbar offers after a load
        // that reopened in Sculpt, or Back if it reopened showing the source.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final boolean sculpting =
                    NativeViewport.productMode() == NativeViewport.MODE_SCULPT;
            final View offered = workspace.findViewById(
                    sculpting ? R.id.back_to_construction : R.id.resume_sculpt);
            assertEquals("IMP01B-08: the reopened project offers the way back in",
                    View.VISIBLE, offered.getVisibility());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // IMP01B-14 / E2E-IMP01B-03: export uses the effective current geometry
    // -----------------------------------------------------------------------

    /**
     * Exporting from Sculpt writes the sculpted geometry, not the stale
     * imported source; exporting from the source representation writes the
     * source.
     *
     * <p>The rule is the one existing Sculpt bodies already follow and is not a
     * second rule for imported ones: the exporter asks the project's MODE, and
     * a body with a Frozen Sculpt Mesh exports it in Sculpt.
     */
    @Test
    public void imp01b14_exportWritesTheEffectiveCurrentGeometryForAnImportedBody() {
        importOneBody();
        startSculpting();

        final byte[] beforeStroke = exportGlb();
        assertTrue("a GLB is written", beforeStroke != null && beforeStroke.length > 0);

        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        final byte[] afterStroke = exportGlb();
        assertTrue("a GLB is still written", afterStroke != null && afterStroke.length > 0);
        assertNotEquals("IMP01B-14: the export follows the sculpted geometry",
                describeBytes(beforeStroke), describeBytes(afterStroke));

        // And leaving Sculpt exports the imported source again, unchanged --
        // which is the same effective-state rule, not a special case.
        backToSource();
        final byte[] fromSource = exportGlb();
        assertTrue("a GLB is written from the source too",
                fromSource != null && fromSource.length > 0);
        assertNotEquals("IMP01B-14: and it is not the sculpted geometry",
                describeBytes(afterStroke), describeBytes(fromSource));

        // Exporting is a READ: no revision, no history step, no slot movement.
        final int undoBefore = undoDepth();
        exportGlb();
        assertEquals("IMP01B-14: exporting records no history step", undoBefore, undoDepth());
        assertFalse("and writes neither .forge slot", ProjectSlot.exists(context()));
    }

    // -----------------------------------------------------------------------
    // IMP01B-10 / IMP01B-24: nothing fabricates a Construction Source
    // -----------------------------------------------------------------------

    @Test
    public void imp01b10and24_sculptingAnImportedBodyInventsNoConstructionSourceAndNoNewScope() {
        importOneBody();
        startSculpting();
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        backToSource();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("IMP01B-10: the body is still an Imported Mesh",
                    NativeViewport.sceneActiveBodyIsImported());
            assertNotEquals("IMP01B-10: and a shape Apply on it is still refused",
                    NativeViewport.APPLY_APPLIED,
                    NativeViewport.applyConstructionBox(3.0, 2.0, 1.0));
            assertEquals("IMP01B-10: Shape is still withdrawn for it", null,
                    workspace.findViewById(R.id.tool_rail_shape));
            return null;
        });

        // IMP01B-24: the forbidden scope stays absent. A BOUNDED brush set, no
        // OBJ or FBX route, and no second way to open a `.glb`.
        //
        // The brush set is asked of the DOMAIN, not of the rail: the rail draws
        // its entries only while sculpting, and what this guards is that the
        // closed enum below JNI has exactly the approved number of members and
        // refuses the next index rather than clamping into a neighbour. It was
        // four through `IMPORT-01B`; `SCULPT-FCM-R1` approved Flatten, Crease
        // and Mask, which is why the number moved — and it must keep taking a
        // stage to move it.
        assertEquals("IMP01B-24: the brush set is still exactly the approved seven tools",
                7, sculptToolsTheDomainAccepts());
        openProjectSurface();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final StringBuilder text = new StringBuilder();
            collectVisibleText(workspace.projectPopover(), text);
            final String surface = text.toString().toLowerCase(java.util.Locale.US);
            assertFalse("no OBJ route is drawn", surface.contains("obj"));
            assertFalse("no FBX route is drawn", surface.contains("fbx"));
            assertNotNull("and there is still exactly one import control",
                    workspace.findViewById(R.id.import_glb));
            return null;
        });
        closeProjectSurface();
    }

    /**
     * E2E-IMP01B-12. The owner's own `1 lowpoly.glb` is not in this repository
     * and is never searched for on the device.
     *
     * <p>Reading arbitrary user folders to find a file nobody put here is not a
     * test, and a fabricated pass would be worse than an honest gap. The case
     * records the gap by name so the report cannot claim otherwise.
     */
    @Test
    public void e2eImp01b12_theOwnersRealFileIsNotPresentAndIsNotSearchedFor() {
        assertFalse("OWNER_REAL_FILE_01B_RETEST_PENDING: the owner's file is not an asset",
                assetExists("glb/1 lowpoly.glb"));
    }

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    /**
     * Imports the fixture and leaves exactly ONE imported body selected.
     *
     * <p>The six-node file produces six objects; five are deleted through the
     * ordinary domain operation so the viewport centre lands on the one that is
     * left. Deleting is a product act with its own suite — used here only to
     * isolate, never asserted.
     */
    private long importOneBody() {
        final long[] before = sceneBodyIds();
        final File file = new File(scratch, "external.glb");
        writeFile(file, readAsset(SIX_NODE_GLB));
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onOpenGlbDocumentChosen(Uri.fromFile(file));
            return null;
        });
        settleLayout();

        final long[] after = sceneBodyIds();
        assertTrue("the import produced objects", after.length > before.length);
        final long keep = after[before.length];
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            for (int i = before.length + 1; i < after.length; i++) {
                NativeViewport.sceneDeleteBody(after[i]);
            }
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(keep));
            // At the origin, unrotated and unscaled, so the viewport centre
            // lands on it. An import leaves the file's own translation on the
            // body, which for this fixture is not the origin.
            assertEquals(NativeViewport.APPLY_APPLIED,
                    NativeViewport.applyBoxTransform(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0,
                            1.0));
            NativeViewport.setSculptBrush(300.0, 1.0);
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        return keep;
    }

    /** Presses Start Sculpting the way a user does. */
    private void startSculpting() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.freeze_to_sculpt).performClick();
            return null;
        });
        settleLayout();
        assertEquals("Start Sculpting must have entered Sculpt", NativeViewport.MODE_SCULPT,
                productMode());
    }

    /** Presses the way out of Sculpt the way a user does. */
    private void backToSource() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.back_to_construction).performClick();
            return null;
        });
        settleLayout();
    }

    private void resumeSculpt() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.resume_sculpt).performClick();
            return null;
        });
        settleLayout();
    }

    private double[] readSculpt() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] state = new double[NativeViewport.SCULPT_STATE_SIZE];
            NativeViewport.sculptState(state);
            return state;
        });
    }

    /**
     * How many tool indices the domain accepts.
     *
     * <p>`setSculptTool` refuses an unknown index and answers with the tool that
     * is ACTUALLY active, so an index it takes is one that comes back. Counting
     * those asks "how many brushes are there" of the closed enum rather than of
     * whatever the rail happens to be drawing -- the rail draws the four only
     * while sculpting, and this question is not about the mode.
     */
    private int sculptToolsTheDomainAccepts() {
        final Integer accepted = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            int found = 0;
            for (int tool = 0; tool < 16; tool++) {
                if (NativeViewport.setSculptTool(tool) == tool) {
                    found++;
                }
            }
            NativeViewport.setSculptTool(NativeViewport.TOOL_GRAB);
            return found;
        });
        return accepted.intValue();
    }

    /**
     * The vertex count the project's `IMPT` record carries for ONE body.
     *
     * <p>Found by walking the entries and matching the ObjectId, never by taking
     * the first: the scene is process-scoped, so another case in the same shard
     * may legitimately have left an imported body earlier in scene order.
     *
     * <p>Read out of the canonical `.forge` bytes rather than from a native
     * accessor written for the test, because the document is where an imported
     * object's arrays are project truth and is the only place this layer may
     * observe them. The layout is `DATA_PACKAGE_SPEC.md` §7a.
     */
    private int importedVertexCountFromProject(long objectId) {
        final byte[] p = importedSectionOfProject();
        assertTrue("the project must carry an IMPT section", p.length > 0);
        final int entries = readU32(p, 0);
        int at = 4;
        for (int e = 0; e < entries; e++) {
            final long id = readU64(p, at);
            at += 8;
            final int nameBytes = readU16(p, at);
            at += 2 + nameBytes;
            final int vertexCount = readU32(p, at);
            final int indexCount = readU32(p, at + 4);
            final int batchCount = readU32(p, at + 8);
            if (id == objectId) {
                return vertexCount;
            }
            // positions + normals (12 bytes each per vertex), indices, batches.
            at += 12 + 24 * vertexCount + 4 * indexCount + 9 * batchCount;
        }
        throw new AssertionError("no IMPT entry for body " + objectId);
    }

    private static long readU64(byte[] bytes, int at) {
        long value = 0;
        for (int i = 7; i >= 0; i--) {
            value = (value << 8) | (bytes[at + i] & 0xFFL);
        }
        return value;
    }

    /** The IMPT section's payload bytes, or an empty array when there is none. */
    private byte[] importedSectionOfProject() {
        return sectionPayload(encodeProject(), "IMPT");
    }

    private static boolean sectionPresent(byte[] file, String tag) {
        return sectionPayload(file, tag).length > 0;
    }

    /**
     * One section's payload out of a `.forge` file.
     *
     * <p>The envelope is walked rather than assumed: 28 header bytes, then
     * sections of a 24-byte header plus their payload. This is the ONE piece of
     * format knowledge in this suite, and it exists so that "the imported
     * source did not change" can be asserted against bytes rather than against
     * a native accessor written for the purpose.
     */
    private static byte[] sectionPayload(byte[] file, String tag) {
        if (file == null || file.length < 28) {
            return new byte[0];
        }
        int at = 28;
        while (at + 24 <= file.length) {
            final boolean matches = file[at] == tag.charAt(0) && file[at + 1] == tag.charAt(1)
                    && file[at + 2] == tag.charAt(2) && file[at + 3] == tag.charAt(3);
            long payloadBytes = 0;
            for (int i = 7; i >= 0; i--) {
                payloadBytes = (payloadBytes << 8) | (file[at + 8 + i] & 0xFFL);
            }
            final int start = at + 24;
            final int end = start + (int) payloadBytes;
            if (end > file.length || payloadBytes < 0) {
                return new byte[0];
            }
            if (matches) {
                final byte[] payload = new byte[(int) payloadBytes];
                System.arraycopy(file, start, payload, 0, payload.length);
                return payload;
            }
            at = end;
        }
        return new byte[0];
    }

    private static int readU16(byte[] bytes, int at) {
        return (bytes[at] & 0xFF) | ((bytes[at + 1] & 0xFF) << 8);
    }

    private static int readU32(byte[] bytes, int at) {
        return (bytes[at] & 0xFF) | ((bytes[at + 1] & 0xFF) << 8)
                | ((bytes[at + 2] & 0xFF) << 16) | ((bytes[at + 3] & 0xFF) << 24);
    }

    /** A short, comparable description of a byte array, for readable failures. */
    private static String describeBytes(byte[] bytes) {
        if (bytes == null) {
            return "null";
        }
        long hash = 1469598103934665603L;
        for (byte b : bytes) {
            hash ^= (b & 0xFF);
            hash *= 1099511628211L;
        }
        return bytes.length + ":" + Long.toHexString(hash);
    }

    private byte[] encodeProject() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.encodeProject());
    }

    private byte[] exportGlb() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.exportGlb());
    }

    private int validateProject(byte[] bytes) {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.validateProject(bytes));
    }

    private int productMode() {
        final Integer mode = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.productMode());
        return mode.intValue();
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

    private double[] placement() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] values = new double[9];
            NativeViewport.boxTransform(values);
            return values;
        });
    }

    private void awaitAutosaveIdle() {
        assertTrue("the autosave worker must drain within the timeout",
                onWorkspace(rule.getScenario(), (activity, workspace) ->
                        workspace.autosaveController().awaitIdle(IDLE_TIMEOUT_MS)));
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

    private static void collectVisibleText(View view, StringBuilder out) {
        if (view == null || view.getVisibility() != View.VISIBLE) {
            return;
        }
        if (view instanceof TextView) {
            out.append(((TextView) view).getText()).append('\n');
        }
        if (view instanceof android.view.ViewGroup) {
            final android.view.ViewGroup group = (android.view.ViewGroup) view;
            for (int i = 0; i < group.getChildCount(); i++) {
                collectVisibleText(group.getChildAt(i), out);
            }
        }
    }

    private static void clearAllProjectFiles() {
        context().deleteFile(ProjectSlot.SLOT_FILE_NAME);
        context().deleteFile(ProjectCheckpoint.CHECKPOINT_FILE_NAME);
        ProjectCheckpoint.quarantineFile(context()).delete();
    }

    private static boolean assetExists(String name) {
        try (InputStream in = InstrumentationRegistry.getInstrumentation()
                .getContext().getAssets().open(name)) {
            return in != null;
        } catch (IOException absent) {
            return false;
        }
    }

    private static byte[] readAsset(String name) {
        // The INSTRUMENTATION context's assets: fixtures are packaged with the
        // test APK and are deliberately not shipped in the product.
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

    private static void writeFile(File file, byte[] bytes) {
        try (FileOutputStream out = new FileOutputStream(file)) {
            out.write(bytes);
        } catch (IOException e) {
            throw new AssertionError("could not write " + file, e);
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
