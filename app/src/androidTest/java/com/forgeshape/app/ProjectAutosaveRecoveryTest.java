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

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;

/**
 * `FSR1B-01..09`: autosave protects work without a write storm, and recovery
 * never replaces anything the user did not ask it to.
 *
 * <p><b>No case here sleeps.</b> Every one waits on
 * {@link AutosaveController#awaitIdle}, which pulls the pending checkpoint
 * forward and then waits behind it on the same single worker thread. A test that
 * slept for the debounce would be betting on wall-clock luck and would pin an
 * implementation detail the product deliberately does not promise.
 */
@RunWith(AndroidJUnit4.class)
public final class ProjectAutosaveRecoveryTest {

    private static final long IDLE_TIMEOUT_MS = 15_000L;

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private static Context context() {
        return InstrumentationRegistry.getInstrumentation().getTargetContext();
    }

    @Before
    public void setUp() {
        clearAllProjectFiles();
        resetToBaselineConstruction(rule.getScenario());
    }

    @After
    public void tearDown() {
        clearAllProjectFiles();
        // The recovery question is process-scoped, and a case that left it
        // "unanswered" would make the NEXT class's first Activity show a
        // recovery prompt over its workspace.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.uiState().recordRecoveryResolved();
            return null;
        });
        resetToBaselineConstruction(rule.getScenario());
    }

    private static void clearAllProjectFiles() {
        context().deleteFile(ProjectSlot.SLOT_FILE_NAME);
        context().deleteFile(ProjectCheckpoint.CHECKPOINT_FILE_NAME);
        ProjectCheckpoint.quarantineFile(context()).delete();
    }

    // -----------------------------------------------------------------------
    // FSR1B-01: canonical bytes, separate slot
    // -----------------------------------------------------------------------

    @Test
    public void fsr1b01_autosaveWritesCanonicalForgeBytesToItsOwnSlot() {
        editConstruction(3.25, 1.5, 0.75);
        awaitAutosaveIdle();

        assertTrue("an edit must produce a checkpoint", ProjectCheckpoint.exists(context()));
        final byte[] checkpoint = ProjectCheckpoint.read(context());
        assertNotNull(checkpoint);

        // The SAME canonical document an explicit Save writes — not a delta, not
        // a journal, not a private encoding. Proved by comparing against a fresh
        // encode of the unchanged project.
        final byte[] canonical = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.encodeProject());
        assertArrayEquals("the checkpoint is the canonical .forge document",
                canonical, checkpoint);
        assertEquals("and the real decoder accepts it", NativeViewport.PROJECT_OK,
                NativeViewport.validateProject(checkpoint));

        // The manual slot is the user's, and autosave never touches it. This is
        // the whole reason there are two files.
        assertFalse("autosave must never write the manual slot",
                ProjectSlot.exists(context()));
    }

    // -----------------------------------------------------------------------
    // FSR1B-02: coalescing, newest wins, no write storm
    // -----------------------------------------------------------------------

    @Test
    public void fsr1b02_manyDirtyGenerationsCoalesceIntoOneWrite() {
        final AutosaveController autosave = controller();
        final int writesBefore = autosave.writeCount();

        // Twenty semantic edits in a row, each one noting. A checkpoint per edit
        // would be twenty writes; the policy is one write of the last state.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            for (int i = 1; i <= 20; i++) {
                NativeViewport.applyConstructionBox(2.0 + i * 0.25, 1.0, 0.5);
                workspace.noteProjectMaybeDirty();
            }
            return null;
        });
        awaitAutosaveIdle();

        assertEquals("twenty dirty generations must cost exactly one write", 1,
                autosave.writeCount() - writesBefore);
        assertTrue("and the requests genuinely outnumbered the writes",
                autosave.requestCount() >= 20);

        // Newest wins: the checkpoint is the LAST state, not an intermediate one.
        final byte[] canonical = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.encodeProject());
        assertArrayEquals("the checkpoint holds the newest generation",
                canonical, ProjectCheckpoint.read(context()));
    }

    @Test
    public void fsr1b02_anUnchangedProjectCostsNoWriteAtAll() {
        editConstruction(4.0, 2.0, 1.0);
        awaitAutosaveIdle();
        final AutosaveController autosave = controller();
        final int writesAfterEdit = autosave.writeCount();
        final int skippedBefore = autosave.skippedUnchangedCount();

        // Note repeatedly with nothing changed. The fingerprint is what makes
        // this free rather than merely fast.
        for (int i = 0; i < 5; i++) {
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                workspace.noteProjectMaybeDirty();
                return null;
            });
            awaitAutosaveIdle();
        }
        assertEquals("an unchanged project is never rewritten", writesAfterEdit,
                autosave.writeCount());
        assertTrue("and the controller says it skipped them",
                autosave.skippedUnchangedCount() > skippedBefore);
    }

    @Test
    public void fsr1b02_aRefusedEditCostsNoWrite() {
        editConstruction(2.5, 1.25, 0.5);
        awaitAutosaveIdle();
        final int writesBefore = controller().writeCount();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            // Refused natively: nothing about the project moves, so nothing
            // about the checkpoint should either.
            assertEquals(NativeViewport.APPLY_REJECTED_NOT_POSITIVE,
                    NativeViewport.applyConstructionBox(-1.0, 1.0, 0.5));
            workspace.noteProjectMaybeDirty();
            return null;
        });
        awaitAutosaveIdle();
        assertEquals("a refused edit writes nothing", writesBefore, controller().writeCount());
    }

    // -----------------------------------------------------------------------
    // FSR1B-03: a failed write never destroys a good checkpoint
    // -----------------------------------------------------------------------

    @Test
    public void fsr1b03_aFailedCheckpointPreservesThePreviousValidOne() {
        editConstruction(5.0, 2.5, 1.25);
        awaitAutosaveIdle();
        final byte[] good = ProjectCheckpoint.read(context());
        assertNotNull("precondition: a valid checkpoint exists", good);

        // Every rejected write, one after another, against a checkpoint that is
        // already there. The atomic write goes to a temporary file first, so a
        // write that never completes cannot have touched the real one.
        assertFalse(ProjectCheckpoint.write(context(), null));
        assertFalse(ProjectCheckpoint.write(context(), new byte[0]));

        assertArrayEquals("the previous valid checkpoint is untouched",
                good, ProjectCheckpoint.read(context()));
        assertEquals("and still decodes", NativeViewport.PROJECT_OK,
                NativeViewport.validateProject(ProjectCheckpoint.read(context())));
        // No half-written file is left lying beside it.
        assertFalse("no pending file survives a failed write",
                new File(context().getFilesDir(), "recovery.forge.pending").exists());
    }

    // -----------------------------------------------------------------------
    // FSR1B-04: a valid candidate is offered exactly once
    // -----------------------------------------------------------------------

    /**
     * The real cold-launch path, through an actual Activity recreation, and the
     * rule that one DECISION is asked once.
     *
     * <p>The distinction the case turns on: an unanswered question that is
     * re-presented after a recreation is still one decision, and suppressing it
     * would leave the user with a candidate they can never answer. What must
     * never happen is being asked again about a decision already made.
     */
    @Test
    public void fsr1b04_aValidCandidateIsOfferedUntilItIsAnsweredAndThenNeverAgain() {
        editConstruction(6.5, 3.0, 1.5);
        awaitAutosaveIdle();
        assertTrue(ProjectCheckpoint.exists(context()));

        // A cold launch: clear the process-scoped answer and rebuild the
        // Activity. The workspace's constructor is the one place the question is
        // ever asked in the product.
        askRecoveryAgain();
        rule.getScenario().recreate();
        settleLayout();
        assertTrue("a validated candidate must be offered",
                onWorkspace(rule.getScenario(),
                        (activity, workspace) -> workspace.recoveryPromptVisible()));
        assertFalse("and the start question waits its turn",
                onWorkspace(rule.getScenario(),
                        (activity, workspace) -> workspace.startChooserVisible()));

        // Rotating while the question is open re-presents the SAME pending
        // decision. Hiding it here would strand the user with a candidate and no
        // way to answer it.
        rule.getScenario().recreate();
        settleLayout();
        assertTrue("an unanswered question survives a recreation",
                onWorkspace(rule.getScenario(),
                        (activity, workspace) -> workspace.recoveryPromptVisible()));

        // Answering settles it for the process.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.recovery_discard).performClick();
            return null;
        });
        settleLayout();
        assertFalse(onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.recoveryPromptVisible()));

        // And a recreation after the answer must not ask again. Being asked
        // twice about a decision already made reads as the app not having
        // believed the user.
        rule.getScenario().recreate();
        settleLayout();
        assertFalse("an answered decision is never presented twice",
                onWorkspace(rule.getScenario(),
                        (activity, workspace) -> workspace.recoveryPromptVisible()));
    }

    @Test
    public void fsr1b04_noCandidateMeansNoQuestion() {
        // The checkpoint is deleted AFTER the queue has drained: the baseline
        // reset is itself a project change, so it legitimately schedules a
        // checkpoint, and deleting before that landed would delete nothing.
        awaitAutosaveIdle();
        assertTrue(ProjectCheckpoint.clear(context()));
        assertFalse(ProjectCheckpoint.exists(context()));

        assertFalse("nothing to recover, nothing to ask",
                onWorkspace(rule.getScenario(),
                        (activity, workspace) -> workspace.offerRecoveryForTest()));
        assertFalse(onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.recoveryPromptVisible()));
    }

    // -----------------------------------------------------------------------
    // FSR1B-05: Recover restores Construction semantics and a fresh history
    // -----------------------------------------------------------------------

    @Test
    public void fsr1b05_recoverRestoresTheExactConstructionValuesAndAFreshHistory() {
        editConstruction(7.25, 3.5, 1.75);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyBoxTransform(1.5, -0.5, 2.25, 370.0, 0.0, 0.0, 1.0, 2.0, 0.5);
            workspace.noteProjectMaybeDirty();
            return null;
        });
        awaitAutosaveIdle();

        // Move the live model well away from the checkpoint, so "the values came
        // back" cannot be true by accident, and give it a history to lose.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionSphere(9.0);
            NativeViewport.applyConstructionSphere(8.0);
            return null;
        });
        assertTrue("precondition: there is a history to clear",
                undoDepth() > 0);

        offerAndPress(R.id.recovery_recover);

        final double[] primitive = primitiveState();
        assertEquals(NativeViewport.PRIMITIVE_BOX,
                (int) primitive[NativeViewport.PRIMITIVE_KIND]);
        assertEquals(7.25, primitive[NativeViewport.PRIMITIVE_BOX_WIDTH], 0.0);
        assertEquals(3.5, primitive[NativeViewport.PRIMITIVE_BOX_WIDTH + 1], 0.0);
        assertEquals(1.75, primitive[NativeViewport.PRIMITIVE_BOX_WIDTH + 2], 0.0);

        final double[] transform = new double[NativeViewport.TRANSFORM_SIZE];
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.boxTransform(transform);
            return null;
        });
        // 370 degrees is stored exactly as given and must not come back as 10.
        assertEquals(370.0, transform[NativeViewport.TRANSFORM_ROTATION], 0.0);
        assertEquals(2.0, transform[NativeViewport.TRANSFORM_SCALE + 1], 0.0);

        assertEquals("a recovered project starts a fresh session history", 0, undoDepth());
        assertFalse("and the candidate is retired once it is the live project",
                ProjectCheckpoint.exists(context()));
    }

    // -----------------------------------------------------------------------
    // FSR1B-06: Recover restores Sculpt semantics, including hasEdits
    // -----------------------------------------------------------------------

    @Test
    public void fsr1b06_recoverRestoresTheSculptMeshAndItsSafetyState() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionSphere(1.0);
            NativeViewport.setSculptBrush(300.0, 1.0);
            NativeViewport.setSculptTool(NativeViewport.TOOL_GRAB);
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.freezeToSculpt());
            return null;
        });
        settleLayout();
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            // A Construction change after the stroke, so the checkpoint also
            // carries a true stale-source flag rather than the default.
            NativeViewport.applyConstructionSphere(1.0);
            workspace.noteProjectMaybeDirty();
            return null;
        });
        awaitAutosaveIdle();

        final double[] before = sculptState();
        assertTrue("precondition: the stroke edited the mesh",
                before[NativeViewport.SCULPT_HAS_EDITS] != 0.0);

        offerAndPress(R.id.recovery_recover);

        final double[] after = sculptState();
        assertEquals("a project checkpointed while sculpting reopens sculpting",
                NativeViewport.MODE_SCULPT, (int) after[NativeViewport.SCULPT_MODE]);
        assertEquals("the same vertex count comes back",
                before[NativeViewport.SCULPT_VERTEX_COUNT],
                after[NativeViewport.SCULPT_VERTEX_COUNT], 0.0);
        assertEquals("and the same topology",
                before[NativeViewport.SCULPT_INDEX_COUNT],
                after[NativeViewport.SCULPT_INDEX_COUNT], 0.0);
        assertEquals("attached to the same body",
                before[NativeViewport.SCULPT_OBJECT_ID],
                after[NativeViewport.SCULPT_OBJECT_ID], 0.0);
        assertEquals("with its stale-source state",
                before[NativeViewport.SCULPT_SOURCE_STALE],
                after[NativeViewport.SCULPT_SOURCE_STALE], 0.0);
        // The destructive Reset-Sculpt-from-Shape guard asks this. A recovered
        // mesh that reported no edits would let that reset discard every stroke
        // in the checkpoint without a word.
        assertTrue("a recovered sculpt mesh still reports user edits",
                after[NativeViewport.SCULPT_HAS_EDITS] != 0.0);
    }

    // -----------------------------------------------------------------------
    // FSR1B-07: Discard touches the checkpoint and nothing else
    // -----------------------------------------------------------------------

    @Test
    public void fsr1b07_discardRemovesTheCandidateAndLeavesTheSavedProjectAlone() {
        // An explicit Save first: this is the file the user would be furious to
        // lose, and Discard must not go near it.
        editConstruction(2.0, 1.0, 0.5);
        saveThroughTheProductControl();
        final byte[] savedProject = ProjectSlot.read(context());
        assertNotNull(savedProject);

        // Then unsaved work on top of it.
        editConstruction(8.5, 4.0, 2.0);
        awaitAutosaveIdle();
        assertTrue(ProjectCheckpoint.exists(context()));

        offerAndPress(R.id.recovery_discard);

        assertFalse("the candidate is gone", ProjectCheckpoint.exists(context()));
        assertArrayEquals("the explicitly saved project is byte-identical",
                savedProject, ProjectSlot.read(context()));
        assertTrue("and the start question is asked now that recovery is settled",
                onWorkspace(rule.getScenario(),
                        (activity, workspace) -> workspace.startChooserVisible())
                        || onWorkspace(rule.getScenario(),
                                (activity, workspace) -> workspace.uiState().startChoiceMade()));
    }

    // -----------------------------------------------------------------------
    // FSR1B-08: a corrupt candidate is non-destructive and does not loop
    // -----------------------------------------------------------------------

    @Test
    public void fsr1b08_aCorruptCandidateIsQuarantinedAndNeverOfferedAgain() {
        editConstruction(2.0, 1.0, 0.5);
        saveThroughTheProductControl();
        final byte[] savedProject = ProjectSlot.read(context());

        editConstruction(9.5, 4.5, 2.25);
        awaitAutosaveIdle();
        final byte[] checkpoint = ProjectCheckpoint.read(context());
        assertNotNull(checkpoint);

        // One bit inside the SCNE payload: every length and count stays right,
        // so only the checksum can catch it.
        final byte[] damaged = checkpoint.clone();
        damaged[28 + 24 + 5] ^= 0x01;
        assertTrue(ProjectCheckpoint.write(context(), damaged));

        final double[] liveBefore = WorkspaceTestSupport.nativeSnapshot();

        assertFalse("a candidate that does not decode is never offered",
                onWorkspace(rule.getScenario(),
                        (activity, workspace) -> workspace.offerRecoveryForTest()));
        assertArrayEquals("and nothing about the live project moved",
                liveBefore, WorkspaceTestSupport.nativeSnapshot(), 0.0);
        assertArrayEquals("nor about the explicitly saved one",
                savedProject, ProjectSlot.read(context()));
        // Quarantined, not left in place: this is the whole do-not-loop rule.
        assertFalse("the broken candidate is out of the candidate position",
                ProjectCheckpoint.exists(context()));
        assertTrue("and is kept aside as the only evidence of what happened",
                ProjectCheckpoint.quarantineFile(context()).isFile());

        // A second launch must not ask either — there is nothing left to ask
        // about, which is what stops the loop.
        assertFalse("and it never comes back",
                onWorkspace(rule.getScenario(),
                        (activity, workspace) -> workspace.offerRecoveryForTest()));
    }

    @Test
    public void fsr1b08_anUnsupportedMajorCandidateIsAlsoQuarantined() {
        editConstruction(3.0, 1.5, 0.75);
        awaitAutosaveIdle();
        final byte[] checkpoint = ProjectCheckpoint.read(context());
        final byte[] newer = checkpoint.clone();
        newer[8] = 2;  // the major, little-endian
        assertTrue(ProjectCheckpoint.write(context(), newer));

        assertFalse("an unsupported major is not a candidate",
                onWorkspace(rule.getScenario(),
                        (activity, workspace) -> workspace.offerRecoveryForTest()));
        assertFalse("and it is moved out of the candidate position",
                ProjectCheckpoint.exists(context()));
    }

    // -----------------------------------------------------------------------
    // FSR1B-09: lifecycle checkpoints the latest state, once
    // -----------------------------------------------------------------------

    @Test
    public void fsr1b09_leavingTheForegroundCheckpointsTheLatestState() {
        editConstruction(4.75, 2.25, 1.125);
        // Deliberately NOT awaiting idle: the point is that stopping asks for a
        // checkpoint immediately rather than waiting out a debounce that a dying
        // process may never see the end of.
        final byte[] expected = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.encodeProject());

        rule.getScenario().moveToState(androidx.lifecycle.Lifecycle.State.CREATED);
        settleLayout();
        awaitAutosaveIdle();

        assertTrue("stopping must leave a checkpoint behind",
                ProjectCheckpoint.exists(context()));
        assertArrayEquals("of the state as it was when the Activity stopped",
                expected, ProjectCheckpoint.read(context()));

        rule.getScenario().moveToState(androidx.lifecycle.Lifecycle.State.RESUMED);
        settleLayout();
    }

    @Test
    public void fsr1b09_recreationPreservesTheProjectAndProducesOneCandidate() {
        editConstruction(5.5, 2.75, 1.375);
        awaitAutosaveIdle();
        final double[] before = WorkspaceTestSupport.nativeSnapshot();
        // Compared against what the scene ACTUALLY holds, not against one. The
        // scene is process-scoped and there is no Delete, so a case that runs
        // after the twenty-body scalability case inherits its bodies — and
        // asserting a literal here would be asserting a test ORDER rather than
        // the claim, which is that a recreation duplicates nothing.
        final int bodiesBefore = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneBodyCount());

        rule.getScenario().recreate();
        settleLayout();

        assertArrayEquals("a recreation changes no project value",
                before, WorkspaceTestSupport.nativeSnapshot(), 0.0);
        assertEquals("and duplicates no body", bodiesBefore,
                (int) onWorkspace(rule.getScenario(),
                        (activity, workspace) -> NativeViewport.sceneBodyCount()));
        assertFalse("nor present a recovery decision it already settled",
                onWorkspace(rule.getScenario(),
                        (activity, workspace) -> workspace.recoveryPromptVisible()));
    }

    // -----------------------------------------------------------------------
    // Shared machinery
    // -----------------------------------------------------------------------

    private AutosaveController controller() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.autosaveController());
    }

    private void awaitAutosaveIdle() {
        assertTrue("the autosave worker must drain within the timeout",
                controller().awaitIdle(IDLE_TIMEOUT_MS));
    }

    private void editConstruction(final double w, final double h, final double d) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionBox(w, h, d);
            workspace.noteProjectMaybeDirty();
            return null;
        });
        settleLayout();
    }

    /**
     * Puts the recovery question back, as a fresh process would.
     *
     * <p>The flag is process-scoped so a recreation cannot re-ask; a test that
     * wants the cold-launch path has to clear it deliberately, which is exactly
     * what a real process death does for free.
     */
    private void askRecoveryAgain() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.uiState().clearRecoveryResolved();
            return null;
        });
    }

    /**
     * Offers the candidate the test planted, then presses one of the answers.
     *
     * <p>Through the workspace seam rather than an Activity recreation, and for
     * a reason that is a property of the product: leaving the foreground
     * checkpoints the live project, so a recreation writes a checkpoint of its
     * own and would race the fixture this case just planted. The DECISION under
     * test is the same one either way -- the seam runs the constructor's own
     * offerRecoveryIfPresent. The constructor path is covered by
     * {@code fsr1b04}, which recreates for real.
     */
    private void offerAndPress(final int rowId) {
        assertTrue("the candidate must be on offer before it can be answered",
                onWorkspace(rule.getScenario(),
                        (activity, workspace) -> workspace.offerRecoveryForTest()));
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(rowId).performClick();
            return null;
        });
        settleLayout();
        assertFalse("answering closes the question",
                onWorkspace(rule.getScenario(),
                        (activity, workspace) -> workspace.recoveryPromptVisible()));
    }

    private void saveThroughTheProductControl() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            if (!workspace.projectPopover().isOpen()) {
                workspace.findViewById(R.id.project_actions_button).performClick();
            }
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.project_save).performClick();
            return null;
        });
        settleLayout();
    }

    private int undoDepth() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.constructionUndoDepth());
    }

    private double[] primitiveState() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] state = new double[NativeViewport.PRIMITIVE_STATE_SIZE];
            NativeViewport.constructionPrimitive(state);
            return state;
        });
    }

    private double[] sculptState() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] state = new double[NativeViewport.SCULPT_STATE_SIZE];
            NativeViewport.sculptState(state);
            return state;
        });
    }
}
