package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * `E2E-SCHNAV-01..09`: the Sculpt History navigator ({@code SCULPT-H1}).
 *
 * <h2>What this suite is for, and what it is not</h2>
 *
 * <p>The DOMAIN half — the cursor model, jump-backward as repeated Undo,
 * jump-forward as repeated Redo, the no-op, the out-of-range refusal, branch
 * invalidation and per-body isolation — is proved by the native self-tests
 * ({@code SCHNAV-01..12}), which build their own sessions and depend on no live
 * state. What is left, and what this covers, is what can only be true on a
 * device: the real control, the surface it opens, the rows a user taps, the
 * five-row viewport target, the rebinding a body switch performs, System Back,
 * and the persistence boundary.
 *
 * <p>Geometry is observed through the {@code .forge} document's {@code SCUL}
 * section, exactly as {@code SculptUndoTest} observes it, and compared
 * bit-exactly — a jump restores stored floats verbatim rather than recomputing
 * them. {@code SCULPT_REVISION} deliberately cannot serve: it is monotonic and
 * goes FORWARD across a backward jump, so it proves that something changed and
 * never what it changed to.
 */
@RunWith(AndroidJUnit4.class)
public final class SculptHistoryNavigatorTest {

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private static Context context() {
        return InstrumentationRegistry.getInstrumentation().getTargetContext();
    }

    @Before
    public void setUp() {
        context().deleteFile(ProjectSlot.SLOT_FILE_NAME);
        context().deleteFile(ProjectCheckpoint.CHECKPOINT_FILE_NAME);
        ProjectCheckpoint.quarantineFile(context()).delete();
        resetToBaselineConstruction(rule.getScenario());
    }

    // -----------------------------------------------------------------------
    // E2E-SCHNAV-01 — Sculpt only, and the branch it lists
    // -----------------------------------------------------------------------

    /**
     * E2E-SCHNAV-01. The control is absent in Construction and present in
     * Sculpt, and the surface it opens lists one row per retained STATE — three
     * strokes are four rows, with the current one marked and last.
     */
    @Test
    public void e2eSchnav01_theControlIsSculptOnlyAndListsOneRowPerRetainedState() {
        assertEquals("E2E-SCHNAV-01: absent in Construction, not drawn and refused",
                View.GONE, navigatorActionVisibility());

        startSculptingASphere();
        assertEquals("E2E-SCHNAV-01: present in Sculpt", View.VISIBLE,
                navigatorActionVisibility());

        openNavigator();
        assertEquals("an empty history is still one state -- the one on screen",
                1, rowCount());
        assertEquals("and the cursor stands on it", 0, currentRowIndex());
        closeNavigator();

        stroke();
        stroke();
        stroke();
        openNavigator();
        assertEquals("E2E-SCHNAV-01: three strokes are four states", 4, rowCount());
        assertEquals("E2E-SCHNAV-01: and the cursor is on the newest", 3, currentRowIndex());
        assertEquals("the branch below JNI agrees with the rows drawn",
                4, historyState()[NativeViewport.SCULPT_HISTORY_STATE_COUNT], 0.0);
    }

    // -----------------------------------------------------------------------
    // E2E-SCHNAV-02 / 03 — a tap is repeated Undo, and repeated Redo
    // -----------------------------------------------------------------------

    /**
     * E2E-SCHNAV-02. Tapping an older row lands on bit-exactly the geometry
     * that tapping Undo that many times produces — measured by running both
     * paths in the same session and comparing {@code SCUL} bytes.
     */
    @Test
    public void e2eSchnav02_tappingAnOlderRowEqualsRepeatedUndo() {
        startSculptingASphere();
        final String seed = sculptDigest();
        stroke();
        stroke();
        stroke();
        final String afterThree = sculptDigest();
        assertNotEquals("precondition: three real strokes moved the mesh", seed, afterThree);

        // The reference path: three taps on the real Undo control.
        clickUndo();
        clickUndo();
        clickUndo();
        final String byRepeatedUndo = sculptDigest();
        assertEquals("precondition: repeated Undo reaches the seed", seed, byRepeatedUndo);

        // Back to the newest state, then one tap on the navigator's oldest row.
        clickRedo();
        clickRedo();
        clickRedo();
        assertEquals("precondition: repeated Redo returns to the newest state",
                afterThree, sculptDigest());

        openNavigator();
        tapRow(0);
        assertEquals("E2E-SCHNAV-02: one tap equals three Undos, bit for bit",
                byRepeatedUndo, sculptDigest());
        assertEquals("and the cursor moved with it", 0, currentRowIndex());
        assertEquals("while the branch kept every row", 4, rowCount());
    }

    /**
     * E2E-SCHNAV-03. And tapping a newer row equals repeated Redo, on the same
     * terms.
     */
    @Test
    public void e2eSchnav03_tappingANewerRowEqualsRepeatedRedo() {
        startSculptingASphere();
        stroke();
        stroke();
        stroke();
        final String afterThree = sculptDigest();

        clickUndo();
        clickUndo();
        clickUndo();
        clickRedo();
        clickRedo();
        clickRedo();
        final String byRepeatedRedo = sculptDigest();
        assertEquals("precondition: the reference path returns to the newest state",
                afterThree, byRepeatedRedo);

        openNavigator();
        tapRow(0);
        assertNotEquals("precondition: the jump moved off the newest state",
                byRepeatedRedo, sculptDigest());
        tapRow(3);
        assertEquals("E2E-SCHNAV-03: one forward tap equals three Redos, bit for bit",
                byRepeatedRedo, sculptDigest());
        assertEquals("and the cursor is back on the newest row", 3, currentRowIndex());
    }

    // -----------------------------------------------------------------------
    // E2E-SCHNAV-04 — the row already stood on
    // -----------------------------------------------------------------------

    /** E2E-SCHNAV-04. Tapping the current row changes nothing at all. */
    @Test
    public void e2eSchnav04_tappingTheCurrentRowChangesNothing() {
        startSculptingASphere();
        stroke();
        stroke();
        openNavigator();

        final String before = sculptDigest();
        final int rowsBefore = rowCount();
        final int cursorBefore = currentRowIndex();
        final double revisionBefore = readSculpt()[NativeViewport.SCULPT_REVISION];
        final double editsBefore = readSculpt()[NativeViewport.SCULPT_HAS_EDITS];

        tapRow(cursorBefore);

        assertEquals("E2E-SCHNAV-04: no geometry moved", before, sculptDigest());
        assertEquals("E2E-SCHNAV-04: the revision did not advance",
                revisionBefore, readSculpt()[NativeViewport.SCULPT_REVISION], 0.0);
        assertEquals("E2E-SCHNAV-04: the edited flag did not move",
                editsBefore, readSculpt()[NativeViewport.SCULPT_HAS_EDITS], 0.0);
        assertEquals("E2E-SCHNAV-04: the branch is unchanged", rowsBefore, rowCount());
        assertEquals("E2E-SCHNAV-04: and so is the cursor", cursorBefore, currentRowIndex());
    }

    // -----------------------------------------------------------------------
    // E2E-SCHNAV-05 — the abandoned future
    // -----------------------------------------------------------------------

    /**
     * E2E-SCHNAV-05. A backward jump leaves the future walkable; the next
     * stroke drops it, through the rule that already existed.
     */
    @Test
    public void e2eSchnav05_aNewStrokeAfterABackwardJumpDropsTheAbandonedFuture() {
        startSculptingASphere();
        stroke();
        stroke();
        stroke();
        openNavigator();
        assertEquals("precondition: four states", 4, rowCount());

        tapRow(1);
        assertEquals("a backward jump keeps every row", 4, rowCount());
        assertEquals("and stands on the one that was tapped", 1, currentRowIndex());
        assertEquals("the future is still walkable",
                2, historyState()[NativeViewport.SCULPT_HISTORY_REDO_COUNT], 0.0);

        // Closed before the stroke: the surface stands on the model, and a
        // stroke aimed at the viewport centre must reach the mesh rather than
        // a panel that happens to be over it on a small window.
        closeNavigator();
        stroke();
        openNavigator();
        assertEquals("E2E-SCHNAV-05: the abandoned future is gone", 3, rowCount());
        assertEquals("E2E-SCHNAV-05: and the new stroke is the newest state",
                2, currentRowIndex());
        assertEquals("nothing is left ahead of the cursor",
                0, historyState()[NativeViewport.SCULPT_HISTORY_REDO_COUNT], 0.0);
    }

    // -----------------------------------------------------------------------
    // E2E-SCHNAV-06 — one body's branch is not another's
    // -----------------------------------------------------------------------

    /**
     * E2E-SCHNAV-06. Switching the active body rebinds the navigator to THAT
     * body's branch, and jumping on one leaves the other exactly where it was —
     * its branch, its cursor and its geometry.
     *
     * <p>Body switching is refused while sculpting, so each hand-over goes out
     * through {@code Back to Construction} and back in through the real
     * controls, which is the route a user takes.
     */
    @Test
    public void e2eSchnav06_switchingBodiesRebindsTheNavigator() {
        startSculptingASphere();
        final long first = activeBodyId();
        stroke();
        stroke();
        stroke();
        openNavigator();
        assertEquals("precondition: the first body has four states", 4, rowCount());
        assertEquals("precondition: standing on its newest", 3, currentRowIndex());
        closeNavigator();

        backToSource();
        final long second = addSphereBody();
        assertTrue("precondition: the two bodies are different", first != second);
        startSculpting();
        stroke();
        // Both bodies now carry a Frozen Sculpt Mesh, so this digest covers the
        // pair: it can only come back unchanged if NEITHER moved.
        final String bothAtRest = sculptDigest();

        openNavigator();
        assertEquals("E2E-SCHNAV-06: the second body has its own two-state branch",
                2, rowCount());
        assertEquals("E2E-SCHNAV-06: and its own cursor", 1, currentRowIndex());

        tapRow(0);
        assertEquals("jumping on the second body moved its cursor", 0, currentRowIndex());
        assertNotEquals("and moved its geometry", bothAtRest, sculptDigest());
        closeNavigator();

        // Back to the first body: its branch and its cursor must be exactly
        // where sculpting it left them.
        backToSource();
        selectBody(first);
        resumeSculpt();
        openNavigator();
        assertEquals("E2E-SCHNAV-06: the first body kept its own four-state branch",
                4, rowCount());
        assertEquals("E2E-SCHNAV-06: and its own cursor, untouched by the other body",
                3, currentRowIndex());
        closeNavigator();

        // And walking the second body forward again restores the pair exactly,
        // which can only hold if the first body's vertices never moved.
        backToSource();
        selectBody(second);
        resumeSculpt();
        openNavigator();
        tapRow(1);
        assertEquals("E2E-SCHNAV-06: the pair is bit-identical again",
                bothAtRest, sculptDigest());
    }

    // -----------------------------------------------------------------------
    // E2E-SCHNAV-07 — the persistence boundary
    // -----------------------------------------------------------------------

    /**
     * E2E-SCHNAV-07. Navigating writes no {@code .forge} byte of its own and
     * records no Construction step.
     *
     * <p>Proved the only way worth proving: a round trip back to the state the
     * project started on encodes BYTE-IDENTICALLY to the document before any of
     * it, so no navigator state, cursor or branch reached the format — and the
     * Construction history's depth is unmoved on both sides, so a sculpt jump
     * did not become a project act.
     */
    @Test
    public void e2eSchnav07_navigatingReachesNeitherTheDocumentNorTheConstructionHistory() {
        startSculptingASphere();
        stroke();
        stroke();

        final byte[] atRest = encodeProject();
        final int constructionDepth = constructionUndoDepth();

        openNavigator();
        tapRow(0);
        tapRow(1);
        tapRow(2);

        assertEquals("E2E-SCHNAV-07: a round trip encodes byte-identically",
                digest(atRest), digest(encodeProject()));
        assertEquals("E2E-SCHNAV-07: and records no Construction step",
                constructionDepth, constructionUndoDepth());
        assertEquals("navigating minted no sculpt history entry either",
                2, historyState()[NativeViewport.SCULPT_HISTORY_UNDO_COUNT]
                        + historyState()[NativeViewport.SCULPT_HISTORY_REDO_COUNT], 0.0);
    }

    // -----------------------------------------------------------------------
    // E2E-SCHNAV-08 — five rows visible, and the rest one scroll away
    // -----------------------------------------------------------------------

    /**
     * E2E-SCHNAV-08. The list shows at most five rows at once and SCROLLS over
     * the rest: the history is not shortened to five, and no row is shrunk
     * below the interactive floor to fit more in.
     */
    @Test
    public void e2eSchnav08_atMostFiveRowsAreVisibleAndTheListScrollsOverTheRest() {
        startSculptingASphere();
        for (int i = 0; i < 7; i++) {
            stroke();
        }
        openNavigator();

        assertEquals("precondition: seven strokes are eight states", 8, rowCount());

        final int cap = onWorkspace(rule.getScenario(), (activity, workspace) ->
                workspace.historyNavigator().listScroller().maxHeightPx());
        final int rowHeight = EditorControlStyles.dimen(context(), R.dimen.control_height);
        assertTrue("E2E-SCHNAV-08: the list is capped at five rows, not at the branch",
                cap > 0 && cap < 8 * rowHeight);

        final int contentHeight = onWorkspace(rule.getScenario(), (activity, workspace) ->
                workspace.historyNavigator().rowsContainer().getHeight());
        final int shownHeight = onWorkspace(rule.getScenario(), (activity, workspace) ->
                workspace.historyNavigator().listScroller().getHeight());
        assertTrue("E2E-SCHNAV-08: eight rows do not fit, so the list must scroll",
                contentHeight > shownHeight && shownHeight > 0);
        assertTrue("E2E-SCHNAV-08: the list can actually be scrolled to the rest",
                onWorkspace(rule.getScenario(), (activity, workspace) ->
                        workspace.historyNavigator().listScroller()
                                .canScrollVertically(1)
                                || workspace.historyNavigator().listScroller()
                                        .canScrollVertically(-1)));

        // Every row is still a full-height target: the list gave up rows, never
        // the interactive floor.
        assertTrue("E2E-SCHNAV-08: no row was shrunk to fit more in",
                onWorkspace(rule.getScenario(), (activity, workspace) -> {
                    final LinearLayout rows = workspace.historyNavigator().rowsContainer();
                    for (int i = 0; i < rows.getChildCount(); i++) {
                        if (rows.getChildAt(i).getHeight() < rowHeight) {
                            return false;
                        }
                    }
                    return rows.getChildCount() == 8;
                }));
    }

    // -----------------------------------------------------------------------
    // E2E-SCHNAV-09 — Back closes the navigator first
    // -----------------------------------------------------------------------

    /**
     * E2E-SCHNAV-09. System Back closes the navigator before it means anything
     * else, and un-lights the control with it — Back has to mean exactly what
     * pressing the control again means.
     */
    @Test
    public void e2eSchnav09_backClosesTheNavigatorFirstAndUnlightsItsControl() {
        startSculptingASphere();
        stroke();
        openNavigator();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("E2E-SCHNAV-09: Back has a surface to close",
                    workspace.hasDismissibleSurface());
            assertTrue("E2E-SCHNAV-09: and the dismissal is ours",
                    workspace.dismissTopmostSurface());
            return null;
        });
        settleLayout();

        assertFalse("E2E-SCHNAV-09: the navigator is closed",
                onWorkspace(rule.getScenario(),
                        (activity, workspace) -> workspace.historyNavigator().isOpen()));
        assertFalse("E2E-SCHNAV-09: and its control is no longer lit",
                onWorkspace(rule.getScenario(), (activity, workspace) ->
                        workspace.historyNavigatorAction().isActivated()));
    }

    // --- driving ------------------------------------------------------------

    private void openNavigator() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            if (!workspace.historyNavigator().isOpen()) {
                workspace.historyNavigatorAction().performClick();
            }
            return null;
        });
        settleLayout();
        assertTrue("the navigator opened", onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.historyNavigator().isOpen()));
    }

    private void closeNavigator() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            if (workspace.historyNavigator().isOpen()) {
                workspace.historyNavigatorAction().performClick();
            }
            return null;
        });
        settleLayout();
    }

    /** Taps a row the way a user does, by its ordinal in the drawn list. */
    private void tapRow(final int ordinal) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final LinearLayout rows = workspace.historyNavigator().rowsContainer();
            assertTrue("row " + ordinal + " must exist to be tapped",
                    ordinal >= 0 && ordinal < rows.getChildCount());
            rows.getChildAt(ordinal).performClick();
            return null;
        });
        settleLayout();
    }

    private int rowCount() {
        return onWorkspace(rule.getScenario(), (activity, workspace) ->
                workspace.historyNavigator().rowsContainer().getChildCount());
    }

    /**
     * Which row is drawn as the current one, read off the ROWS rather than off
     * native state — so the drawn list is what is asserted, not the model it
     * was built from.
     */
    private int currentRowIndex() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final LinearLayout rows = workspace.historyNavigator().rowsContainer();
            int found = -1;
            for (int i = 0; i < rows.getChildCount(); i++) {
                final TextView row = (TextView) rows.getChildAt(i);
                if (row.isActivated()) {
                    assertEquals("exactly one row may be marked current", -1, found);
                    found = i;
                }
            }
            return found;
        });
    }

    private int navigatorActionVisibility() {
        return onWorkspace(rule.getScenario(), (activity, workspace) ->
                workspace.historyNavigatorAction().getVisibility());
    }

    private double[] historyState() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] state = new double[NativeViewport.SCULPT_HISTORY_STATE_SIZE];
            NativeViewport.sculptHistoryState(state);
            return state;
        });
    }

    private void stroke() {
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        settleLayout();
    }

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

    private void startSculptingASphere() {
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

    private static void assertAccepted(String what, int status) {
        assertTrue(what + " must be accepted, not rejected (status " + status + ")",
                status == NativeViewport.APPLY_APPLIED
                        || status == NativeViewport.APPLY_UNCHANGED);
    }

    // --- reads --------------------------------------------------------------

    private long activeBodyId() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneActiveBodyId());
    }

    /** Unboxed so an int/Integer overload is never ambiguous at a call site. */
    private int productMode() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.productMode());
    }

    private int constructionUndoDepth() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.constructionUndoDepth());
    }

    private double[] readSculpt() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] state = new double[NativeViewport.SCULPT_STATE_SIZE];
            NativeViewport.sculptState(state);
            return state;
        });
    }

    private byte[] encodeProject() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.encodeProject());
    }

    /**
     * The whole SCUL section: where sculpt geometry is project truth.
     *
     * <p>Whole rather than sliced per body, deliberately. The section carries
     * every frozen body, so an unchanged digest is a statement about ALL of
     * them at once — which is the stronger claim when what is being proved is
     * that one body's jump did not reach another's vertices.
     */
    private String sculptDigest() {
        return digest(sectionPayload(encodeProject(), "SCUL"));
    }

    private void resumeSculpt() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.resume_sculpt).performClick();
            return null;
        });
        settleLayout();
        assertEquals("Resume Sculpt must have re-entered Sculpt", NativeViewport.MODE_SCULPT,
                productMode());
    }

    private void selectBody(final long id) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(id));
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    /**
     * One section's payload out of a `.forge` file: 28 envelope bytes, then
     * sections of a 24-byte header plus payload. `DATA_PACKAGE_SPEC.md` owns
     * the layout; this is the same walk {@code SculptUndoTest} performs.
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

    private static String digest(byte[] bytes) {
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
}
