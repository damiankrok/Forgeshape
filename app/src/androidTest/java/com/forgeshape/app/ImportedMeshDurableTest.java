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
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
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

/**
 * `IMP01A-14/15/21/22/23/24/25` and `E2E-IMP01A-01..12`: durable GLB import.
 *
 * <h2>What changed, and what this suite is for</h2>
 *
 * <p>`GLB-IMPORT-R0/R1` read a `.glb` into a session-only preview that owned no
 * project state at all. `IMPORT-01A` makes the same parse produce REAL objects:
 * an ObjectId from the scene's own allocator, a row in the Objects list, the
 * ordinary Move/Rotate/Scale gizmo, one Undo step for the whole import, and
 * geometry the `.forge` document carries so the project reopens without the
 * source file.
 *
 * <p>The domain half of that — how many objects a file becomes, what they are
 * called, the transform split, atomicity, the codec — is proved by the native
 * self-test suites, which build their own scenes and depend on no live session.
 * What is left, and what this suite covers, is everything that can only be true
 * on a device: the workspace surfaces, the two `.forge` slots, autosave and
 * recovery, Scoped Storage transfer, and process death.
 *
 * <h2>What must still be absent</h2>
 *
 * <p>An imported object is NON-PARAMETRIC. It has no Construction Source and
 * nothing may invent one, so <i>Shape</i> has no answer for it; and
 * `IMPORT-01A` gives it no Frozen Sculpt Mesh, so <i>Start Sculpting</i> on one
 * is `IMPORT-01B`. Both controls are withdrawn while one is selected, and both
 * are refused below JNI as well — withdrawing a control is not removing its
 * guard.
 */
@RunWith(AndroidJUnit4.class)
public final class ImportedMeshDurableTest {

    /**
     * A ForgeShape export of the six-body Construction corpus project.
     *
     * <p>Six top-level mesh nodes, which is what makes it the multi-object
     * fixture: an import of it must produce six rows and not one, and not
     * thirty-six.
     */
    private static final String SIX_NODE_GLB = "glb/construction_sentinel.glb";

    private static final int SIX_NODE_OBJECTS = 6;

    /** How long the autosave worker gets to drain. Never slept for; see awaitIdle. */
    private static final long IDLE_TIMEOUT_MS = 5000L;

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private final File scratch = new File(context().getCacheDir(), "import-01a-test");

    private static Context context() {
        return InstrumentationRegistry.getInstrumentation().getTargetContext();
    }

    /**
     * The project as this case found it, captured before anything was imported.
     *
     * <p>The scene is process-scoped and this is the only suite that adds a
     * representation later cases do not expect, so every case puts the project
     * back through the ordinary atomic load rather than leaving imported bodies
     * behind for whatever runs next. It is a product path, not a test seam:
     * loading a document replaces the scene exactly, which is precisely the
     * cleanup wanted.
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
            workspace.uiState().recordRecoveryResolved();
            if (baselineProject != null) {
                NativeViewport.loadProject(baselineProject);
            }
            return null;
        });
        resetToBaselineConstruction(rule.getScenario());
    }

    // -----------------------------------------------------------------------
    // E2E-IMP01A-01: a real picker result becomes durable objects
    // -----------------------------------------------------------------------

    @Test
    public void e2eImp01a01_aPickedGlbBecomesDurableObjectsInTheObjectsList() {
        final long[] before = sceneBodyIds();
        final int undoBefore = undoDepth();

        importThroughThePickerResult(readAsset(SIX_NODE_GLB));

        final long[] after = sceneBodyIds();
        assertEquals("six mesh nodes became six objects, appended to the scene",
                before.length + SIX_NODE_OBJECTS, after.length);
        for (int i = 0; i < before.length; i++) {
            assertEquals("the bodies that were there keep their order and identity",
                    before[i], after[i]);
        }
        for (int i = before.length; i < after.length; i++) {
            assertTrue("every new body is an Imported Mesh",
                    NativeViewport.sceneBodyIsImported(after[i]));
        }
        assertEquals("the whole import is exactly one Undo step", undoBefore + 1, undoDepth());
        assertEquals("and the first object it created is selected",
                after[before.length], NativeViewport.sceneActiveBodyId());

        // E2E-IMP01A-05: primitives are not rows. The fixture's six nodes are
        // six objects however many TRIANGLES primitives each of them carries.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openObjectsPanel(workspace);
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final ObjectsSectionView objects = objectsSection(workspace);
            assertEquals("one row per body, and no row per primitive",
                    after.length, objects.rowCount());
            for (long id : after) {
                assertNotNull("every body has a row, found by tag and never by position",
                        objects.rowFor(id));
            }
            WorkspaceTestSupport.closeObjectsPanel(workspace);
            return null;
        });
        settleLayout();
    }

    // -----------------------------------------------------------------------
    // IMP01A-23 / E2E-IMP01A-02: rows are selectable and name themselves
    // -----------------------------------------------------------------------

    @Test
    public void imp01a23_anImportedRowIsSelectableAndCarriesTheFilesName() {
        importThroughThePickerResult(readAsset(SIX_NODE_GLB));
        final long[] ids = sceneBodyIds();
        final long last = ids[ids.length - 1];

        final String name = NativeViewport.sceneBodyName(last);
        assertFalse("an imported body carries the name the file gave it", name.isEmpty());
        assertFalse("and a Construction Body still carries none",
                !NativeViewport.sceneBodyName(ids[0]).isEmpty());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openObjectsPanel(workspace);
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View row = objectsSection(workspace).rowFor(last);
            assertNotNull(row);
            assertEquals("the row reads the file's name, not Body #id",
                    name, ((android.widget.TextView) row).getText().toString());
            row.performClick();
            return null;
        });
        settleLayout();
        assertEquals("selecting the row makes that body active",
                last, NativeViewport.sceneActiveBodyId());
        assertTrue(NativeViewport.sceneActiveBodyIsImported());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.closeObjectsPanel(workspace);
            return null;
        });
        settleLayout();
    }

    // -----------------------------------------------------------------------
    // IMP01A-14/15 / E2E-IMP01A-10: Shape and Start Sculpting are withdrawn
    // -----------------------------------------------------------------------

    @Test
    public void imp01a14and15_shapeAndStartSculptingAreAbsentForAnImportedBody() {
        // First, with a Construction Body selected, both are offered — so the
        // assertions below cannot pass by the controls never being there.
        //
        // The sculpt transition is asserted as "one of the two is drawn"
        // deliberately. Start Sculpting and Resume Sculpt are mutually
        // exclusive by meaning — there is nothing to resume until something has
        // been frozen — so which of them a Construction Body offers depends on
        // whether it already has a Frozen Sculpt Mesh, which is a fact about
        // the body rather than about this case.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertNotNull("Shape is offered for a Construction Body",
                    workspace.findViewById(R.id.tool_rail_shape));
            assertTrue("and so is a sculpt transition", sculptTransitionOffered(workspace));
            return null;
        });

        importThroughThePickerResult(readAsset(SIX_NODE_GLB));

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(NativeViewport.sceneActiveBodyIsImported());
            assertNull("Shape has no answer for an imported body and is not drawn",
                    workspace.findViewById(R.id.tool_rail_shape));
            assertNotNull("Transform stays, because a placement is a placement",
                    workspace.findViewById(R.id.tool_rail_place));
            // NEITHER transition is drawn: Start Sculpting is `IMPORT-01B`'s,
            // and an imported body has no Frozen Sculpt Mesh to resume into.
            assertFalse("no sculpt transition is drawn for an imported body",
                    sculptTransitionOffered(workspace));
            return null;
        });

        // The guard is still in the DOMAIN: removing a control is not removing
        // a guard, so both acts are refused below JNI as well.
        final byte[] before = encodeProject();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertNotEquals("a shape Apply on an imported body is refused",
                    NativeViewport.APPLY_APPLIED,
                    NativeViewport.applyConstructionBox(3.0, 2.0, 1.0));
            assertNotEquals("and so is a freeze", NativeViewport.SCULPT_OK,
                    NativeViewport.freezeToSculpt());
            return null;
        });
        assertArrayEquals("and neither changed one byte of the project",
                before, encodeProject());

        // Selecting a Construction Body again brings both back untouched.
        final long constructionBody = sceneBodyIds()[0];
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.sceneSelectBody(constructionBody);
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertNotNull("Shape comes back for a Construction Body",
                    workspace.findViewById(R.id.tool_rail_shape));
            assertTrue("and so does the sculpt transition",
                    sculptTransitionOffered(workspace));
            return null;
        });
    }

    /**
     * Whether the toolbar offers a way into Sculpt for the active body.
     *
     * <p>Start Sculpting and Resume Sculpt are mutually exclusive by meaning, so
     * which one is drawn depends on whether the body already has a Frozen Sculpt
     * Mesh. What `IMPORT-01A` asserts is that an Imported Mesh offers NEITHER,
     * and a Construction Body still offers one — not which one.
     */
    private static boolean sculptTransitionOffered(EditorWorkspaceView workspace) {
        final View freeze = workspace.findViewById(R.id.freeze_to_sculpt);
        final View resume = workspace.findViewById(R.id.resume_sculpt);
        return (freeze != null && freeze.getVisibility() == View.VISIBLE)
                || (resume != null && resume.getVisibility() == View.VISIBLE);
    }

    // -----------------------------------------------------------------------
    // E2E-IMP01A-03/04: Move, Rotate and Scale, then Undo and Redo
    // -----------------------------------------------------------------------

    @Test
    public void e2eImp01a03and04_moveRotateAndScaleOnAnImportedBodyUndoAndRedoExactly() {
        importThroughThePickerResult(readAsset(SIX_NODE_GLB));
        final long imported = NativeViewport.sceneActiveBodyId();
        assertTrue(NativeViewport.sceneBodyIsImported(imported));

        final double[] placed = placement();
        final byte[] geometryBefore = encodeProject();

        // One Apply carries all nine values, which is one history step.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.APPLY_APPLIED,
                    NativeViewport.applyBoxTransform(2.5, -1.0, 0.75, 15.0, 37.5, -8.25,
                            1.5, 0.5, 2.0));
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        final double[] moved = placement();
        assertNotEquals("the placement moved", placed[0], moved[0], 1e-9);
        assertEquals(37.5, moved[4], 1e-9);
        assertEquals(2.0, moved[8], 1e-9);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.constructionUndo());
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        assertArrayEquals("an undo returns the placement the file gave it",
                placed, placement(), 1e-9);
        assertArrayEquals("and moves not one byte of the imported geometry",
                geometryBefore, encodeProject());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.constructionRedo());
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        assertArrayEquals("and a redo returns the edited one exactly",
                moved, placement(), 1e-9);
        assertTrue("the body is still the same imported body",
                NativeViewport.sceneBodyIsImported(imported)
                        && NativeViewport.sceneActiveBodyId() == imported);
    }

    /** E2E-IMP01A-03: undoing the IMPORT itself removes every object it made. */
    @Test
    public void e2eImp01a03_undoingTheImportRemovesEveryObjectItCreated() {
        final long[] before = sceneBodyIds();
        final byte[] projectBefore = encodeProject();

        importThroughThePickerResult(readAsset(SIX_NODE_GLB));
        assertEquals(before.length + SIX_NODE_OBJECTS, sceneBodyIds().length);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.constructionUndo());
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        assertArrayEquals("one undo removes every object the import created",
                before, sceneBodyIds());
        assertSameProjectExceptTheIdAllocator(projectBefore, encodeProject());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.constructionRedo());
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        final long[] again = sceneBodyIds();
        assertEquals("and a redo brings all of them back", before.length + SIX_NODE_OBJECTS,
                again.length);
        for (int i = before.length; i < again.length; i++) {
            assertTrue(NativeViewport.sceneBodyIsImported(again[i]));
        }
    }

    // -----------------------------------------------------------------------
    // IMP01A-16/17/18 / E2E-IMP01A-06: the file carries the geometry
    // -----------------------------------------------------------------------

    @Test
    public void e2eImp01a06_aSavedProjectReopensWithoutTheSourceGlb() {
        importThroughThePickerResult(readAsset(SIX_NODE_GLB));
        final long[] expectedIds = sceneBodyIds();
        final byte[] saved = encodeProject();

        saveThroughTheProductControl();
        assertTrue("Save wrote the manual slot", ProjectSlot.exists(context()));
        assertArrayEquals("and it is the canonical document",
                saved, ProjectSlot.read(context()));

        // Which bodies are imported, recorded before the scene is replaced.
        final boolean[] wasImported = new boolean[expectedIds.length];
        for (int i = 0; i < expectedIds.length; i++) {
            wasImported[i] = NativeViewport.sceneBodyIsImported(expectedIds[i]);
        }

        // The scene is genuinely REPLACED first, so reopening has something to
        // restore. Done through the ordinary atomic load of the project this
        // case started from — nothing else on the device is the source `.glb`,
        // and nothing has to be: the geometry is IN the `.forge` file.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.PROJECT_OK, NativeViewport.loadProject(baselineProject));
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        assertNotEquals("the imported objects really are gone before the reopen",
                expectedIds.length, sceneBodyIds().length);

        openSavedProjectThroughTheProductControl();
        assertArrayEquals("every body comes back, in scene order, with its identity",
                expectedIds, sceneBodyIds());
        for (int i = 0; i < expectedIds.length; i++) {
            assertEquals("and with the representation it had",
                    wasImported[i], NativeViewport.sceneBodyIsImported(expectedIds[i]));
        }
        assertArrayEquals("re-encoding the reopened project is byte-identical",
                saved, encodeProject());
    }

    /**
     * IMP01A-22 / E2E-IMP01A-08. Save Copy and Open File carry an imported body
     * exactly as they carry every other kind, and the file keeps no trace of
     * where it came from.
     */
    @Test
    public void imp01a22_saveCopyAndOpenFilePreserveAnImportedBody() {
        importThroughThePickerResult(readAsset(SIX_NODE_GLB));
        final byte[] expected = encodeProject();
        final long[] expectedIds = sceneBodyIds();

        final File copy = new File(scratch, "a distinctively named copy.forge");
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onSaveCopyRequestedForTest(expected);
            workspace.onCreateProjectDocumentChosen(Uri.fromFile(copy));
            return null;
        });
        settleLayout();
        assertTrue("the copy exists", copy.isFile());
        assertArrayEquals("and is the same canonical document",
                expected, readFile(copy));

        resetToBaselineConstruction(rule.getScenario());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onOpenProjectDocumentChosen(Uri.fromFile(copy));
            return null;
        });
        settleLayout();
        assertArrayEquals("opening the copy restores every body",
                expectedIds, sceneBodyIds());
        assertArrayEquals("and re-encoding it carries no trace of the path it came from",
                expected, encodeProject());
        assertFalse("opening a file does not write the internal manual slot",
                ProjectSlot.exists(context()));
    }

    // -----------------------------------------------------------------------
    // IMP01A-21 / E2E-IMP01A-07: autosave and recovery
    // -----------------------------------------------------------------------

    @Test
    public void imp01a21_autosaveCheckpointsAnImportedBodyAndRecoveryRestoresIt() {
        importThroughThePickerResult(readAsset(SIX_NODE_GLB));
        awaitAutosaveIdle();

        assertTrue("an import is a project change and is checkpointed",
                ProjectCheckpoint.exists(context()));
        final byte[] checkpoint = ProjectCheckpoint.read(context());
        assertNotNull(checkpoint);
        assertArrayEquals("the checkpoint is the same canonical document",
                encodeProject(), checkpoint);
        assertEquals("and the real fail-closed decoder accepts it",
                NativeViewport.PROJECT_OK, NativeViewport.validateProject(checkpoint));
        assertFalse("autosave still never writes the manual slot",
                ProjectSlot.exists(context()));

        final long[] expectedIds = sceneBodyIds();
        resetToBaselineConstruction(rule.getScenario());
        // Recovery is the ordinary load path over the ordinary decoder — no
        // second format and no second reader.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.PROJECT_OK, NativeViewport.loadProject(checkpoint));
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        assertArrayEquals("recovery restores the imported objects with no source file",
                expectedIds, sceneBodyIds());
        assertArrayEquals(checkpoint, encodeProject());
    }

    /**
     * IMP01A-19. The committed corpus fixtures — written by the INDEPENDENT
     * PowerShell encoder, not by this codec — decode and load on the device.
     *
     * <p>The C++ self-test compares digests, which proves the two encoders
     * agree. This proves the other half: that the bytes that agreement produced
     * are a project this build can actually open, with the representations the
     * specification says they carry.
     */
    @Test
    public void imp01a19_theCommittedImportedFixturesLoadOnTheDevice() {
        loadFixtureAndAssert("forge/imported_only_v1.forge", 1, new boolean[]{true});
        loadFixtureAndAssert("forge/construction_imported_v1.forge", 2,
                new boolean[]{false, true});
        loadFixtureAndAssert("forge/mixed_imported_v1.forge", 3,
                new boolean[]{false, false, true});

        // And the legacy fixtures still decode exactly as they did, which is
        // what "the imported branch costs a project that has none nothing"
        // means in practice.
        loadFixtureAndAssert("forge/construction_multibody_v1.forge", 6,
                new boolean[]{false, false, false, false, false, false});
        loadFixtureAndAssert("forge/sculpt_mixed_v1.forge", 2, new boolean[]{false, false});
    }

    private void loadFixtureAndAssert(String asset, int expectedBodies,
                                      boolean[] expectedImported) {
        final byte[] bytes = readAsset(asset);
        assertEquals(asset + " must pass the real fail-closed decoder",
                NativeViewport.PROJECT_OK, NativeViewport.validateProject(bytes));
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(asset + " must load", NativeViewport.PROJECT_OK,
                    NativeViewport.loadProject(bytes));
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        final long[] ids = sceneBodyIds();
        assertEquals(asset + " body count", expectedBodies, ids.length);
        for (int i = 0; i < ids.length; i++) {
            assertEquals(asset + " body " + i + " representation",
                    expectedImported[i], NativeViewport.sceneBodyIsImported(ids[i]));
        }
        // Re-encoding what was loaded reproduces the committed bytes, with one
        // documented exception: the id allocator is only ever pushed FORWARD,
        // so a fixture whose high-water mark is BELOW where this process has
        // already minted comes back with the process's larger value. That is
        // the rule that stops a post-load creation colliding with a loaded
        // body, and it is the only field allowed to differ.
        assertSameProjectIgnoringTheIdAllocator(asset, bytes, encodeProject());
    }

    // -----------------------------------------------------------------------
    // IMP01A-24 / E2E-IMP01A-09: cancel and failure change nothing
    // -----------------------------------------------------------------------

    @Test
    public void imp01a24_cancelAndFailureAreNoOpsAndSuccessDirtiesTheProject() {
        final double[] snapshotBefore = WorkspaceTestSupport.nativeSnapshot();
        final byte[] projectBefore = encodeProject();
        final long fingerprintBefore = fingerprint();
        final int undoBefore = undoDepth();
        final long[] idsBefore = sceneBodyIds();

        // Whatever the baseline reset earned is not what this case is about.
        // Cleared HERE, after the "before" state is captured and before the
        // refusals, so the checkpoint assertion below is about the refusals.
        awaitAutosaveIdle();
        clearAllProjectFiles();

        // Cancel: the picker came back with nothing.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onOpenGlbDocumentChosen(null);
            return null;
        });
        settleLayout();
        assertArrayEquals("a cancelled import changes no project byte",
                projectBefore, encodeProject());
        assertEquals("no fingerprint movement", fingerprintBefore, fingerprint());
        assertEquals("and no history step", undoBefore, undoDepth());

        // A file the reader will not take: a `.forge` document is not a GLB.
        final File notAGlb = new File(scratch, "not-a-glb.glb");
        writeFile(notAGlb, readAsset("forge/construction_multibody_v1.forge"));
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onOpenGlbDocumentChosen(Uri.fromFile(notAGlb));
            return null;
        });
        settleLayout();
        assertArrayEquals("a refused import changes nothing either",
                projectBefore, encodeProject());
        assertArrayEquals(idsBefore, sceneBodyIds());
        assertEquals(fingerprintBefore, fingerprint());
        assertEquals(undoBefore, undoDepth());
        assertEquals("", WorkspaceTestSupport.describeSnapshotDifference(
                snapshotBefore, WorkspaceTestSupport.nativeSnapshot()));
        awaitAutosaveIdle();
        assertFalse("and no checkpoint was written for a refusal",
                ProjectCheckpoint.exists(context()));

        // A file that is missing entirely, which never reaches the parser.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onOpenGlbDocumentChosen(Uri.fromFile(new File(scratch, "absent.glb")));
            return null;
        });
        settleLayout();
        assertArrayEquals(projectBefore, encodeProject());

        // Success, by contrast, IS a project change.
        importThroughThePickerResult(readAsset(SIX_NODE_GLB));
        assertNotEquals("a successful import dirties the project",
                fingerprintBefore, fingerprint());
        awaitAutosaveIdle();
        assertTrue("and earns a checkpoint", ProjectCheckpoint.exists(context()));
    }

    // -----------------------------------------------------------------------
    // IMP01A-25 / E2E-IMP01A-11: nothing else moved
    // -----------------------------------------------------------------------

    @Test
    public void imp01a25_theWorkspaceKeepsItsAcceptedCompositionAroundAnImport() {
        importThroughThePickerResult(readAsset(SIX_NODE_GLB));

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            // The single right contextual surface is still the one host, still
            // on screen, and still carrying the Tool Rail and the precision
            // trigger. `IMPORT-01A` withdrew one rail ENTRY, not the surface.
            final View host = WorkspaceTestSupport.trailingHost(workspace);
            assertEquals("the trailing host is still drawn", View.VISIBLE,
                    host.getVisibility());
            assertNotNull("the Tool Rail is still inside it",
                    WorkspaceTestSupport.toolRail(workspace));
            assertNotNull("and so is the precision trigger",
                    WorkspaceTestSupport.precisionToggle(workspace));
            assertEquals("Undo and Redo are still on screen", View.VISIBLE,
                    workspace.historyGroup().getVisibility());

            // The interactive floor is a HIT AREA and it did not move.
            final float density = activity.getResources().getDisplayMetrics().density;
            final int floor = Math.round(48f * density);
            final View place = workspace.findViewById(R.id.tool_rail_place);
            assertTrue("the Transform rail entry still meets the 48 dp floor",
                    place.getHeight() >= floor - 1 && place.getWidth() >= floor - 1);
            return null;
        });

        // And no OBJ or FBX route appeared beside the one GLB route.
        openProjectSurface();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertNotNull("there is exactly one import control",
                    workspace.findViewById(R.id.import_glb));
            final StringBuilder text = new StringBuilder();
            collectVisibleText(workspace.projectPopover(), text);
            final String surface = text.toString().toLowerCase(java.util.Locale.US);
            assertFalse("no OBJ route is drawn", surface.contains("obj"));
            assertFalse("no FBX route is drawn", surface.contains("fbx"));
            return null;
        });
        closeProjectSurface();
    }

    /**
     * E2E-IMP01A-12. The owner's own `1 lowpoly.glb` is not in this repository
     * and is never searched for on the device: reading arbitrary user folders is
     * not something a test does. The real-file retest stays the owner's manual
     * step, and this case records that plainly rather than leaving its absence
     * to be discovered in a report.
     */
    @Test
    public void e2eImp01a12_theOwnerRealFileRetestIsPending() {
        assertTrue("OWNER_REAL_FILE_01A_RETEST_PENDING — not a technical failure", true);
    }

    // -----------------------------------------------------------------------
    // Shared machinery
    // -----------------------------------------------------------------------

    /**
     * Drives the REAL picker-result seam: bytes on the device, a {@code Uri} the
     * system would have handed back, and the production handler that turns one
     * into the other. Only the system's own document UI is skipped, because it
     * is another app's surface and cannot be driven reliably.
     */
    private void importThroughThePickerResult(byte[] bytes) {
        final File file = new File(scratch, "external-" + bytes.length + ".glb");
        writeFile(file, bytes);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onOpenGlbDocumentChosen(Uri.fromFile(file));
            return null;
        });
        settleLayout();
    }

    /**
     * Asserts two encoded projects describe the same scene, allowing only the
     * id allocator's high-water mark to differ.
     *
     * <p>Undoing a creation restores the bodies and deliberately does NOT roll
     * the allocator back: an undone import's ids are never handed out again,
     * because reuse is what would let a stale ObjectId held in a selection or a
     * render snapshot resolve to a different body. So the SCNE payload's
     * {@code nextObjectId} — eight bytes at file offset 56 — legitimately
     * differs, and the section's CRC differs with it. Everything else, every
     * body's identity, order and placement included, must be identical.
     */
    private static void assertSameProjectExceptTheIdAllocator(byte[] before, byte[] after) {
        assertSameProjectIgnoringTheIdAllocator("the project", before, after);
        boolean allocatorMoved = false;
        for (int i = 56; i < 64; i++) {
            allocatorMoved = allocatorMoved || before[i] != after[i];
        }
        assertTrue("and the id allocator must have moved FORWARD, never back",
                allocatorMoved);
    }

    /**
     * The same comparison without requiring the allocator to have moved.
     *
     * <p>The SCNE payload's {@code nextObjectId} lives at file offset 56 and its
     * section CRC at 44; everything else — every body's identity, order,
     * representation and placement, and every other section — must be identical
     * byte for byte.
     */
    private static void assertSameProjectIgnoringTheIdAllocator(String what, byte[] before,
                                                                byte[] after) {
        assertEquals(what + ": the document's shape must not change",
                before.length, after.length);
        for (int i = 0; i < before.length; i++) {
            final boolean isSceneCrc = i >= 44 && i < 48;
            final boolean isNextObjectId = i >= 56 && i < 64;
            if (isSceneCrc || isNextObjectId) {
                continue;
            }
            assertEquals(what + ": byte " + i + " must be unchanged", before[i], after[i]);
        }
    }

    private static ObjectsSectionView objectsSection(EditorWorkspaceView workspace) {
        final View view = workspace.findViewById(R.id.objects_section);
        assertNotNull("the Objects section is on screen", view);
        return (ObjectsSectionView) view;
    }

    private long[] sceneBodyIds() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final long[] ids = new long[NativeViewport.sceneBodyCount()];
            NativeViewport.sceneBodyIds(ids);
            return ids;
        });
    }

    private byte[] encodeProject() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.encodeProject());
    }

    private long fingerprint() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.projectFingerprint());
    }

    private int undoDepth() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.constructionUndoDepth());
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

    private void saveThroughTheProductControl() {
        openProjectSurface();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.project_save).performClick();
            return null;
        });
        settleLayout();
    }

    private void openSavedProjectThroughTheProductControl() {
        openProjectSurface();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.project_open).performClick();
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

    private static void collectVisibleText(View view, StringBuilder out) {
        if (view == null || view.getVisibility() != View.VISIBLE) {
            return;
        }
        if (view instanceof android.widget.TextView) {
            out.append(((android.widget.TextView) view).getText()).append('\n');
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

    private static byte[] readAsset(String name) {
        // The INSTRUMENTATION context's assets, not the app's: fixtures are
        // packaged with the test APK and are deliberately not shipped in the
        // product.
        try (InputStream in = InstrumentationRegistry.getInstrumentation()
                .getContext().getAssets().open(name)) {
            final ByteArrayOutputStream out = new ByteArrayOutputStream();
            final byte[] buffer = new byte[8192];
            int read;
            while ((read = in.read(buffer)) > 0) {
                out.write(buffer, 0, read);
            }
            return out.toByteArray();
        } catch (IOException e) {
            throw new AssertionError("asset " + name + " must be packaged", e);
        }
    }

    private static byte[] readFile(File file) {
        try (InputStream in = new java.io.FileInputStream(file)) {
            final ByteArrayOutputStream out = new ByteArrayOutputStream();
            final byte[] buffer = new byte[8192];
            int read;
            while ((read = in.read(buffer)) > 0) {
                out.write(buffer, 0, read);
            }
            return out.toByteArray();
        } catch (IOException e) {
            throw new AssertionError("could not read " + file, e);
        }
    }

    private static void writeFile(File file, byte[] bytes) {
        try (FileOutputStream out = new FileOutputStream(file)) {
            out.write(bytes);
        } catch (IOException e) {
            throw new AssertionError("could not write " + file, e);
        }
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
