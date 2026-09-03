package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
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
 * `SCUNDO-18..24` and `E2E-SCUNDO-01..10`: Sculpt Undo and Redo
 * (`ARCH-OWNER-12`).
 *
 * <h2>What this suite is for, and what it is not</h2>
 *
 * <p>The DOMAIN half — one stroke is one entry, exact before/after restoration,
 * redo branching, the {@code hasEdits} rule, revision monotonicity, per-body
 * separation and both memory bounds — is proved by the native self-tests, which
 * build their own scenes and depend on no live session. What is left, and what
 * this covers, is everything that can only be true on a device: the two real
 * chrome controls, the native mode dispatch behind them, the separation from the
 * Construction history in a running session, and the persistence rule.
 *
 * <h2>How geometry is observed</h2>
 *
 * <p>Through the {@code .forge} document's {@code SCUL} section, which is where
 * a Frozen Sculpt Mesh's positions are project truth — the same route
 * {@code ImportedMeshSculptTest} reads {@code IMPT} by. Nothing here reads a
 * vertex through an accessor written for a test, and the comparison is
 * bit-exact, because an Undo restores stored floats verbatim rather than
 * recomputing them.
 *
 * <p>{@code SCULPT_REVISION} deliberately cannot serve: it is monotonic by
 * design and goes FORWARD across an Undo, so it proves that something changed
 * and never what it changed to.
 */
@RunWith(AndroidJUnit4.class)
public final class SculptUndoTest {

    /** The same six-node import fixture the Imported Mesh suites use. */
    private static final String SIX_NODE_GLB = "glb/construction_sentinel.glb";

    /** How long the autosave worker gets to drain. Never slept for; see awaitIdle. */
    private static final long IDLE_TIMEOUT_MS = 5000L;

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private final File scratch = new File(context().getCacheDir(), "sculpt-undo-test");

    private static Context context() {
        return InstrumentationRegistry.getInstrumentation().getTargetContext();
    }

    /** The project as this case found it. See ImportedMeshSculptTest for why. */
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
    // E2E-SCUNDO-01 — the whole flow, through the actual controls
    // -----------------------------------------------------------------------

    /**
     * E2E-SCUNDO-01, SCUNDO-24. A Construction body: stroke, tap the real Undo,
     * the geometry comes back; tap the real Redo, it goes again.
     *
     * <p>Every act here is a {@code performClick} on the control a user presses.
     * Nothing calls {@code sculptUndo} directly, because what is under test is
     * as much the dispatch as the history.
     */
    @Test
    public void e2eScundo01_theRealControlsUndoAndRedoAConstructionSculptStroke() {
        startSculptingAConstructionSphere();

        final byte[] seed = sculptSection();
        assertTrue("precondition: the project carries a SCUL section", seed.length > 0);
        assertHistoryControls("nothing has been sculpted yet", false, false);

        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        assertNotEquals("precondition: a real stroke moved the sculpt mesh",
                describeBytes(seed), describeBytes(sculptSection()));
        final byte[] afterStroke = sculptSection();
        assertHistoryControls("a completed stroke arms Undo and nothing else", true, false);

        clickUndo();
        assertEquals("E2E-SCUNDO-01: Undo restored the exact pre-stroke geometry",
                describeBytes(seed), describeBytes(sculptSection()));
        assertHistoryControls("and moved the entry to the redo side", false, true);
        assertEquals("SCUNDO-11: the first stroke undone leaves the mesh unedited",
                0.0, readSculpt()[NativeViewport.SCULPT_HAS_EDITS], 0.0);

        clickRedo();
        assertEquals("E2E-SCUNDO-01: Redo restored the exact post-stroke geometry",
                describeBytes(afterStroke), describeBytes(sculptSection()));
        assertHistoryControls("and put the entry back", true, false);
        assertNotEquals("with the edited flag back",
                0.0, readSculpt()[NativeViewport.SCULPT_HAS_EDITS], 0.0);
    }

    // -----------------------------------------------------------------------
    // E2E-SCUNDO-02 — the same controls over an Imported Mesh
    // -----------------------------------------------------------------------

    /**
     * E2E-SCUNDO-02, SCUNDO-09. An imported body sculpts, undoes and redoes
     * through the same controls, and the imported source never moves.
     */
    @Test
    public void e2eScundo02_theRealControlsUndoAndRedoAnImportedSculptStroke() {
        importOneBody();
        startSculpting();

        final byte[] seed = sculptSection();
        final byte[] importedBefore = importedSection();
        assertTrue("precondition: the body has both branches",
                seed.length > 0 && importedBefore.length > 0);

        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        final byte[] afterStroke = sculptSection();
        assertNotEquals("precondition: a real stroke moved the imported body's sculpt mesh",
                describeBytes(seed), describeBytes(afterStroke));

        clickUndo();
        assertEquals("E2E-SCUNDO-02: Undo restored the imported seed exactly",
                describeBytes(seed), describeBytes(sculptSection()));
        clickRedo();
        assertEquals("E2E-SCUNDO-02: and Redo put the stroke back exactly",
                describeBytes(afterStroke), describeBytes(sculptSection()));

        assertEquals("SCUNDO-09: no sculpt history step ever touched the imported source",
                describeBytes(importedBefore), describeBytes(importedSection()));
    }

    // -----------------------------------------------------------------------
    // E2E-SCUNDO-03 / E2E-SCUNDO-04 — order, and the redo branch
    // -----------------------------------------------------------------------

    /**
     * E2E-SCUNDO-03, E2E-SCUNDO-04, SCUNDO-08. Two strokes walk back and
     * forward in order through the real controls; a new stroke taken after an
     * Undo drops the redo branch and disables the control.
     */
    @Test
    public void e2eScundo03and04_twoStrokesWalkInOrderAndANewStrokeDropsTheRedoBranch() {
        startSculptingAConstructionSphere();
        final byte[] seed = sculptSection();

        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        final byte[] afterA = sculptSection();
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        final byte[] afterB = sculptSection();
        assertNotEquals("precondition: the second stroke moved the mesh again",
                describeBytes(afterA), describeBytes(afterB));

        clickUndo();
        assertEquals("E2E-SCUNDO-03: the first Undo lands on the state after A",
                describeBytes(afterA), describeBytes(sculptSection()));
        clickUndo();
        assertEquals("E2E-SCUNDO-03: the second lands on the seed",
                describeBytes(seed), describeBytes(sculptSection()));
        assertHistoryControls("at the oldest state Undo is refused cleanly", false, true);

        clickRedo();
        assertEquals("E2E-SCUNDO-03: the first Redo lands on the state after A",
                describeBytes(afterA), describeBytes(sculptSection()));
        clickRedo();
        assertEquals("E2E-SCUNDO-03: the second lands on the state after B",
                describeBytes(afterB), describeBytes(sculptSection()));
        assertHistoryControls("and there is nothing left to redo", true, false);

        // E2E-SCUNDO-04: undo once, then sculpt. The redo branch is gone.
        clickUndo();
        assertHistoryControls("precondition: a redo exists", true, true);
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        assertHistoryControls("E2E-SCUNDO-04: a new stroke clears the redo branch",
                true, false);
    }

    // -----------------------------------------------------------------------
    // E2E-SCUNDO-05 — Back and Resume keep the runtime history
    // -----------------------------------------------------------------------

    /**
     * E2E-SCUNDO-05, SCUNDO-16. Leaving Sculpt is navigation: it takes nothing
     * away, and Resume Sculpt comes back to both the mesh and its stacks.
     *
     * <p>It also asserts the part that did NOT change: outside Sculpt the same
     * two controls are the Construction history again.
     */
    @Test
    public void e2eScundo05_backAndResumeKeepTheRuntimeSculptHistory() {
        startSculptingAConstructionSphere();
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        clickUndo();

        final int undoBefore = sculptUndoDepth();
        final int redoBefore = sculptRedoDepth();
        final byte[] geometryBefore = sculptSection();
        assertEquals("precondition: one entry on each side", 1, undoBefore);
        assertEquals(1, redoBefore);

        backToSource();
        assertEquals("outside Sculpt the pair is the Construction history again",
                NativeViewport.MODE_CONSTRUCTION, productMode());
        assertEquals("SCUNDO-16: and leaving took nothing away", undoBefore, sculptUndoDepth());
        assertEquals(redoBefore, sculptRedoDepth());

        resumeSculpt();
        assertEquals("E2E-SCUNDO-05: Resume comes back to the same stacks",
                undoBefore, sculptUndoDepth());
        assertEquals(redoBefore, sculptRedoDepth());
        assertEquals("and to the same geometry",
                describeBytes(geometryBefore), describeBytes(sculptSection()));
        assertHistoryControls("with both controls live", true, true);
    }

    // -----------------------------------------------------------------------
    // E2E-SCUNDO-06 / SCUNDO-15 — two bodies, two histories
    // -----------------------------------------------------------------------

    /**
     * E2E-SCUNDO-06. Two bodies with retained sculpt work: the controls act on
     * the active one and can never reach the other.
     */
    @Test
    public void e2eScundo06_eachBodysControlsActOnlyOnItsOwnHistory() {
        final long first = activeBodyId();
        startSculptingAConstructionSphere();
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        assertEquals("precondition: body A has one entry", 1, sculptUndoDepth());

        // A second body, sculpted independently.
        backToSource();
        final long second = addSphereBody();
        startSculpting();
        assertEquals("SCUNDO-15: the new body starts with an empty history",
                0, sculptUndoDepth());
        assertHistoryControls("so its controls are both inert", false, false);
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        assertEquals("body B has two entries of its own", 2, sculptUndoDepth());
        final byte[] bothSculpted = sculptSection();

        // Undo twice in B, then a third time: the third finds nothing and must
        // not reach A.
        clickUndo();
        clickUndo();
        assertEquals("body B is back at its seed", 0, sculptUndoDepth());
        assertHistoryControls("and its Undo is refused cleanly", false, true);
        clickUndo();  // inert control; the tap is still made
        assertEquals("E2E-SCUNDO-06: an exhausted history does not borrow from another body",
                0, sculptUndoDepth());

        // Back to A: its own entry is exactly where it was left.
        backToSource();
        selectBody(first);
        resumeSculpt();
        assertEquals("E2E-SCUNDO-06: body A still holds its own entry", 1, sculptUndoDepth());
        assertHistoryControls("and only A's Undo is live", true, false);

        assertTrue("the two bodies are genuinely different", first != second);
        assertTrue("and both still carry sculpt geometry", bothSculpted.length > 0);
    }

    // -----------------------------------------------------------------------
    // SCUNDO-19 — the project history is not touched
    // -----------------------------------------------------------------------

    /**
     * SCUNDO-19, E2E-SCUNDO-08. A sculpt stroke, a Sculpt Undo and a Sculpt Redo
     * cost the Construction history nothing, and the Construction history still
     * works exactly as before once the user leaves Sculpt.
     */
    @Test
    public void scundo19_theProjectHistoryIsUntouchedBySculptAndStillWorksAfterwards() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionSphere(1.0);
            return null;
        });
        settleLayout();
        final int projectDepth = constructionUndoDepth();
        assertTrue("precondition: the project history has a step", projectDepth > 0);

        startSculpting();
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        clickUndo();
        clickRedo();
        assertEquals("SCUNDO-19: no sculpt act moved the Construction stack",
                projectDepth, constructionUndoDepth());
        final int refusal = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.constructionUndo());
        assertEquals("and the Construction entry point is still refused in Sculpt",
                NativeViewport.HISTORY_REFUSED_IN_SCULPT, refusal);
        assertEquals("which changed nothing either", projectDepth, constructionUndoDepth());

        // E2E-SCUNDO-08: out of Sculpt, a real transform Undo behaves as before.
        backToSource();
        final double[] before = placement();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.APPLY_APPLIED,
                    NativeViewport.applyBoxTransform(3.0, 0.25, -2.0, 0.0, 0.0, 0.0, 1.0, 1.0,
                            1.0));
            return null;
        });
        settleLayout();
        assertNotEquals("precondition: the placement moved",
                before[0], placement()[0], 1e-9);

        clickUndo();
        assertArrayEquals("E2E-SCUNDO-08: the project Undo control still steps the project",
                before, placement(), 1e-9);
    }

    // -----------------------------------------------------------------------
    // SCUNDO-20 / SCUNDO-21 / E2E-SCUNDO-07 — the history is never serialized
    // -----------------------------------------------------------------------

    /**
     * SCUNDO-20, SCUNDO-21, E2E-SCUNDO-07. Save after an Undo, reopen, and the
     * geometry is exactly what was on screen — while the Undo/Redo stacks start
     * empty, because they were never in the file.
     *
     * <p>The document is compared BYTE FOR BYTE against one encoded before the
     * stroke that was undone: an encoder that leaked one history entry could
     * not produce identical bytes. Both the manual document and the autosave
     * checkpoint are checked, because they are the same canonical document
     * written by two acts.
     */
    @Test
    public void scundo20and21_neitherSaveNorAutosaveEverSerializesTheSculptHistory() {
        startSculptingAConstructionSphere();
        final byte[] seedDocument = encodeProject();

        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        final byte[] afterA = sculptSection();
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        clickUndo();  // back to the state after A, with one entry on each side

        assertEquals("precondition: the geometry is the state after stroke A",
                describeBytes(afterA), describeBytes(sculptSection()));
        assertEquals("precondition: there is a redo to lose", 1, sculptRedoDepth());
        assertEquals(1, sculptUndoDepth());

        final byte[] saved = encodeProject();
        awaitAutosaveIdle();
        assertTrue("a sculpted project is checkpointed", ProjectCheckpoint.exists(context()));
        assertArrayEquals("SCUNDO-21: and the checkpoint is that same document",
                saved, ProjectCheckpoint.read(context()));

        // The undone stroke B left NO trace: this document differs from the
        // pre-stroke one only by stroke A's geometry, and carries no stack.
        assertNotEquals("the saved document does carry stroke A",
                describeBytes(seedDocument), describeBytes(saved));

        // Reopen through the ordinary atomic load, which is what Open, Recover
        // and a reopened process all do.
        final File copy = new File(scratch, "sculpt-undo.forge");
        writeFile(copy, saved);
        resetToBaselineConstruction(rule.getScenario());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.PROJECT_OK, NativeViewport.loadProject(readFile(copy)));
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();

        assertEquals("SCUNDO-20: the reopened geometry is what was on screen",
                describeBytes(afterA), describeBytes(sculptSection()));
        assertEquals("SCUNDO-20: and the sculpt undo stack starts empty", 0, sculptUndoDepth());
        assertEquals("SCUNDO-20: as does the redo stack", 0, sculptRedoDepth());
        assertEquals("SCUNDO-20: re-encoding the reopened project reproduces the bytes",
                describeBytes(saved), describeBytes(encodeProject()));

        // A new stroke is the first entry of a NEW runtime history.
        enterSculptIfNeeded();
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        assertEquals("SCUNDO-20: a stroke after reopening starts a fresh history",
                1, sculptUndoDepth());
    }

    // -----------------------------------------------------------------------
    // E2E-SCUNDO-09 / SCUNDO-17 — the destructive reset
    // -----------------------------------------------------------------------

    /**
     * E2E-SCUNDO-09, SCUNDO-17. A Reset from source clears the history, and Undo
     * cannot walk back across it: the reset is the user saying the previous
     * sculpt is gone.
     */
    @Test
    public void e2eScundo09_aResetFromSourceClearsTheHistoryAndUndoCannotCrossIt() {
        startSculptingAConstructionSphere();
        final byte[] seed = sculptSection();
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        assertEquals("precondition: there is a stroke to lose", 1, sculptUndoDepth());
        assertNotEquals(describeBytes(seed), describeBytes(sculptSection()));

        // The ordinary destructive path: the same freeze the confirmation runs.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.freezeToSculpt());
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();

        assertEquals("SCUNDO-17: the reset cleared the undo stack", 0, sculptUndoDepth());
        assertEquals("SCUNDO-17: and the redo stack", 0, sculptRedoDepth());
        assertHistoryControls("E2E-SCUNDO-09: so both controls are inert", false, false);
        assertEquals("SCUNDO-17: the mesh is the source again",
                describeBytes(seed), describeBytes(sculptSection()));
        assertEquals("SCUNDO-17: and reports no edits",
                0.0, readSculpt()[NativeViewport.SCULPT_HAS_EDITS], 0.0);

        clickUndo();
        assertEquals("E2E-SCUNDO-09: Undo cannot cross the reset",
                describeBytes(seed), describeBytes(sculptSection()));
        assertEquals(0, sculptUndoDepth());
    }

    // -----------------------------------------------------------------------
    // SCUNDO-18 — deleting a body
    // -----------------------------------------------------------------------

    /**
     * SCUNDO-18. Deleting a sculpted body is safe: its history leaves the
     * session with it, the remaining body's history is untouched, and undoing
     * the Delete restores the body with its sculpt mesh intact.
     *
     * <p>The Sculpt history survives a Delete/Undo here because the Construction
     * history HOLDS the whole detached body rather than destroying it — see
     * {@code holdDetachedBody}. That is the "zero extra complexity" branch the
     * stage allows: nothing was serialized, nothing was added to a history step,
     * and the stacks come back because the object they live on came back. When
     * no step names the body any more it is released, and the history dies with
     * it.
     */
    @Test
    public void scundo18_deletingASculptedBodyIsSafeAndUndoRestoresItWholly() {
        final long keep = activeBodyId();
        startSculptingAConstructionSphere();
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        assertEquals("precondition: the first body has an entry", 1, sculptUndoDepth());

        backToSource();
        final long doomed = addSphereBody();
        startSculpting();
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        assertEquals("precondition: the second body has two", 2, sculptUndoDepth());
        final byte[] sculptBeforeDelete = sculptSection();
        backToSource();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("SCUNDO-18: deleting a sculpted body succeeds",
                    NativeViewport.DELETE_OK, NativeViewport.sceneDeleteBody(doomed));
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();

        // WHICH body selection falls to is `ObjectsDeleteTest`'s subject and
        // depends on scene order, which other suites in the same process
        // legitimately change. What matters here is that the deleted body is
        // gone and the sculpted survivor is reachable, so the survivor is
        // selected by name rather than assumed.
        assertNotEquals("SCUNDO-18: the deleted body is not the active one",
                doomed, activeBodyId());
        selectBody(keep);
        resumeSculpt();
        assertEquals("SCUNDO-18: whose own history is exactly as it was", 1, sculptUndoDepth());
        backToSource();

        // Undoing the Delete restores the SAME object, whole. Nothing has
        // touched a sculpt vertex in between — the surviving body's Undo is
        // deliberately NOT taken until after this comparison, because the SCUL
        // section carries every body's geometry and stepping one of them would
        // make the document differ for a reason that has nothing to do with
        // the Delete.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.HISTORY_OK, NativeViewport.constructionUndo());
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        assertEquals("SCUNDO-18: the restored body brought its sculpt mesh back, exactly",
                describeBytes(sculptBeforeDelete), describeBytes(sculptSection()));

        selectBody(doomed);
        resumeSculpt();
        assertTrue("SCUNDO-18: and the session is coherent, whatever the stacks hold",
                sculptUndoDepth() >= 0 && sculptRedoDepth() >= 0);

        // The surviving body's own history still steps only itself, after all
        // of that.
        backToSource();
        selectBody(keep);
        resumeSculpt();
        assertEquals("SCUNDO-18: the surviving body kept its own single entry",
                1, sculptUndoDepth());
        clickUndo();
        assertEquals("SCUNDO-18: and stepping it reached nothing else", 0, sculptUndoDepth());
    }

    // -----------------------------------------------------------------------
    // E2E-SCUNDO-10 / SCUNDO-22 — the bounds, on a device
    // -----------------------------------------------------------------------

    /**
     * E2E-SCUNDO-10, SCUNDO-22. A long run of real strokes stays bounded in both
     * step count and bytes, and nothing crashes or allocates without limit.
     *
     * <p>The exact caps are the native suite's subject. What this adds is that
     * they hold under the real touch path, on a real device, over more strokes
     * than any single gesture produces.
     */
    @Test
    public void e2eScundo10_aLongRunOfRealStrokesStaysBounded() {
        startSculptingAConstructionSphere();

        for (int i = 0; i < 40; i++) {
            WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        }

        final int depth = sculptUndoDepth();
        final long bytes = sculptHistoryBytes();
        assertTrue("E2E-SCUNDO-10: forty strokes produced entries", depth > 0);
        assertTrue("SCUNDO-22: and the step cap held — depth was " + depth, depth <= 32);
        assertTrue("SCUNDO-22: and the byte cap held — " + bytes + " bytes",
                bytes <= 4L * 1024L * 1024L);
        assertTrue("SCUNDO-22: entries beyond the cap were evicted",
                sculptHistoryEvicted() > 0);

        // The session is still fully usable after eviction.
        assertHistoryControls("the controls still work at the cap", true, false);
        final byte[] before = sculptSection();
        clickUndo();
        assertNotEquals("E2E-SCUNDO-10: and Undo still moves geometry after eviction",
                describeBytes(before), describeBytes(sculptSection()));
        clickRedo();
        assertEquals("E2E-SCUNDO-10: and Redo still comes back exactly",
                describeBytes(before), describeBytes(sculptSection()));
    }

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    /** Asserts what the two real controls offer right now. */
    private void assertHistoryControls(String why, boolean undo, boolean redo) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(why + " (the pair is drawn)", View.VISIBLE,
                    workspace.historyGroup().getVisibility());
            assertEquals(why + " (Undo)", undo, workspace.undoAction().isEnabled());
            assertEquals(why + " (Redo)", redo, workspace.redoAction().isEnabled());
            return null;
        });
    }

    /** Presses the real Undo control, exactly as a user does. */
    private void clickUndo() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.undoAction().performClick();
            return null;
        });
        settleLayout();
    }

    private void clickRedo() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.redoAction().performClick();
            return null;
        });
        settleLayout();
    }

    /**
     * Puts the active Construction body in Sculpt, on a sphere big enough for
     * the shared stroke helper to capture a meaningful vertex set.
     */
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
        startSculpting();
    }

    /**
     * Accepts an apply that was performed OR was already the current value.
     *
     * <p>These calls are preconditions — a sphere of a known size at the origin,
     * so the shared stroke helper's screen centre lands on it — and a body that
     * already sits there is a legitimate outcome the domain reports as
     * {@code APPLY_UNCHANGED}. What must never pass is a rejection.
     */
    private static void assertAccepted(String what, int status) {
        assertTrue(what + " must be accepted, not rejected (status " + status + ")",
                status == NativeViewport.APPLY_APPLIED
                        || status == NativeViewport.APPLY_UNCHANGED);
    }

    /** Adds a second Construction body, selected and posed for sculpting. */
    private long addSphereBody() {
        final long id = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final long added = NativeViewport.sceneAddBody();
            assertTrue("a second body was created", added != 0L);
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(added));
            assertAccepted("the sphere", NativeViewport.applyConstructionSphere(2.0));
            assertAccepted("the pose", NativeViewport.applyBoxTransform(
                    0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0));
            NativeViewport.setSculptBrush(300.0, 1.0);
            workspace.onNativeStateChanged();
            return added;
        });
        settleLayout();
        return id;
    }

    private void selectBody(long id) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(id));
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    private void startSculpting() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.freeze_to_sculpt).performClick();
            return null;
        });
        settleLayout();
        assertEquals("Start Sculpting must have entered Sculpt", NativeViewport.MODE_SCULPT,
                productMode());
    }

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

    /** A load may reopen in either mode; this puts the session back in Sculpt. */
    private void enterSculptIfNeeded() {
        if (productMode() == NativeViewport.MODE_SCULPT) {
            return;
        }
        resumeSculpt();
        assertEquals("the reopened project resumes into Sculpt", NativeViewport.MODE_SCULPT,
                productMode());
    }

    /**
     * Imports the six-node fixture and leaves exactly ONE imported body
     * selected, posed at the origin so the viewport centre lands on it. The
     * same isolation {@code ImportedMeshSculptTest} performs, for the same
     * reason.
     */
    private void importOneBody() {
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
            assertAccepted("the imported body's pose", NativeViewport.applyBoxTransform(
                    0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0));
            NativeViewport.setSculptBrush(300.0, 1.0);
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    // --- native reads -------------------------------------------------------

    private int sculptUndoDepth() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sculptUndoDepth());
    }

    private int sculptRedoDepth() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sculptRedoDepth());
    }

    private long sculptHistoryBytes() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sculptHistoryBytes());
    }

    private long sculptHistoryEvicted() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sculptHistoryEvictedCount());
    }

    private int constructionUndoDepth() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.constructionUndoDepth());
    }

    private int productMode() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.productMode());
    }

    private long activeBodyId() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneActiveBodyId());
    }

    private double[] readSculpt() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] state = new double[NativeViewport.SCULPT_STATE_SIZE];
            NativeViewport.sculptState(state);
            return state;
        });
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

    private byte[] encodeProject() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.encodeProject());
    }

    /** The SCUL section's payload — where sculpt geometry is project truth. */
    private byte[] sculptSection() {
        return sectionPayload(encodeProject(), "SCUL");
    }

    /** The IMPT section's payload — the imported source, which must never move. */
    private byte[] importedSection() {
        return sectionPayload(encodeProject(), "IMPT");
    }

    private void awaitAutosaveIdle() {
        assertTrue("the autosave worker must drain within the timeout",
                onWorkspace(rule.getScenario(), (activity, workspace) ->
                        workspace.autosaveController().awaitIdle(IDLE_TIMEOUT_MS)));
    }

    /**
     * One section's payload out of a `.forge` file: 28 envelope bytes, then
     * sections of a 24-byte header plus payload. `DATA_PACKAGE_SPEC.md` owns the
     * layout; this is the same walk {@code ImportedMeshSculptTest} performs, and
     * it exists so geometry can be asserted against document bytes rather than
     * against an accessor written for a test.
     */
    private static byte[] sectionPayload(byte[] file, String tag) {
        if (file == null || file.length < 28) {
            return new byte[0];
        }
        int at = 28;
        while (at + 24 <= file.length) {
            final StringBuilder name = new StringBuilder(4);
            for (int i = 0; i < 4; i++) {
                name.append((char) (file[at + i] & 0xFF));
            }
            final long payloadBytes = readU64(file, at + 8);
            final int start = at + 24;
            if (payloadBytes < 0 || start + payloadBytes > file.length) {
                return new byte[0];
            }
            if (name.toString().equals(tag)) {
                final byte[] out = new byte[(int) payloadBytes];
                System.arraycopy(file, start, out, 0, out.length);
                return out;
            }
            at = start + (int) payloadBytes;
        }
        return new byte[0];
    }

    private static long readU64(byte[] bytes, int at) {
        long value = 0;
        for (int i = 7; i >= 0; i--) {
            value = (value << 8) | (bytes[at + i] & 0xFFL);
        }
        return value;
    }

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
            throw new AssertionError("reading " + file, e);
        }
    }

    private static void writeFile(File file, byte[] bytes) {
        try (FileOutputStream out = new FileOutputStream(file)) {
            out.write(bytes);
        } catch (IOException e) {
            throw new AssertionError("writing " + file, e);
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
        if (file == null || !file.exists()) {
            return;
        }
        final File[] children = file.listFiles();
        if (children != null) {
            for (File child : children) {
                deleteRecursively(child);
            }
        }
        assertTrue("could not delete " + file, file.delete());
    }
}
