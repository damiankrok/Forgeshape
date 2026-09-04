package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.describeSnapshotDifference;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.nativeSnapshot;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.graphics.Rect;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * E2ER1A-01, 04, 05 and 06: the product's project affordance, and what a
 * refused Open costs.
 *
 * <p>Every control here is located by its stable semantic id. Nothing is found
 * by screen coordinate: the workspace re-arranges itself per window, so a
 * coordinate is only ever true for one run.
 *
 * <p>The corruption cases matter more than the happy path. A save that works is
 * a feature; a damaged file that leaves the user's work exactly where it was is
 * the guarantee the whole fail-closed load exists for, and it is the one a
 * regression would silently take away.
 */
@RunWith(AndroidJUnit4.class)
public final class EditorWorkspaceProjectActionsTest {

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    @Before
    public void setUp() {
        resetToBaselineConstruction(rule.getScenario());
    }

    @After
    public void tearDown() {
        // The slot is process-wide state that outlives this class, and a later
        // suite must not inherit a project this one injected damage into.
        context().deleteFile(ProjectSlot.SLOT_FILE_NAME);
        resetToBaselineConstruction(rule.getScenario());
    }

    private static Context context() {
        return InstrumentationRegistry.getInstrumentation().getTargetContext();
    }

    // -----------------------------------------------------------------------
    // E2ER1A-01
    // -----------------------------------------------------------------------

    @Test
    public void e2er1a01_saveWritesTheSlotFromARealUserAction() {
        context().deleteFile(ProjectSlot.SLOT_FILE_NAME);
        assertTrue("precondition: the slot must start empty",
                !ProjectSlot.exists(context()));

        final Rect hostBefore = trailingHostBounds();
        openProjectSurface();
        final Rect hostWithSurfaceOpen = trailingHostBounds();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View save = workspace.findViewById(R.id.project_save);
            assertNotNull("the Save control must exist by semantic id", save);
            save.performClick();
            return null;
        });
        settleLayout();

        assertTrue("a real Save action must write a non-empty .forge to the slot",
                ProjectSlot.exists(context()));
        assertTrue("the file must be a plausible project rather than a stub",
                ProjectSlot.slotFile(context()).length() > 64L);
        // The bytes are the codec's, and a file the codec cannot read back is
        // not a save. Proved by loading them straight back.
        assertEquals("the saved bytes must be loadable", NativeViewport.PROJECT_OK,
                NativeViewport.loadProject(ProjectSlot.read(context())));

        assertEquals("the accepted R2 right host must not move when the project"
                        + " surface opens", hostBefore, hostWithSurfaceOpen);
        assertEquals("...nor after the save", hostBefore, trailingHostBounds());
    }

    @Test
    public void e2er1a01_bothProjectControlsCarryTheInteractiveFloor() {
        openProjectSurface();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final float density = workspace.getResources().getDisplayMetrics().density;
            final int floor = Math.round(48f * density);
            for (int id : new int[]{R.id.project_actions_button, R.id.project_save,
                    R.id.project_open}) {
                final View control = workspace.findViewById(id);
                assertNotNull("control " + id + " must exist", control);
                assertTrue("control " + id + " must reach the 48 dp hit area in height,"
                                + " measured " + control.getHeight(),
                        control.getHeight() >= floor);
                assertTrue("control " + id + " must reach the 48 dp hit area in width,"
                                + " measured " + control.getWidth(),
                        control.getWidth() >= floor);
            }
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // E2ER1A-04 and E2ER1A-05: a refused Open costs the user nothing
    // -----------------------------------------------------------------------

    @Test
    public void e2er1a04_aDamagedProjectIsRefusedAndChangesNothing() {
        final byte[] saved = saveAndRead();
        // One bit inside the SCNE payload. Every length and every count stays
        // right, so only the checksum can catch it — which is exactly the class
        // of damage a truncation check would miss.
        final byte[] damaged = saved.clone();
        damaged[28 + 24 + 5] ^= 0x01;
        assertRefusedOpenChangesNothing(damaged, R.string.status_project_damaged);
    }

    @Test
    public void e2er1a04_aTruncatedProjectIsRefusedAndChangesNothing() {
        final byte[] saved = saveAndRead();
        final byte[] truncated = new byte[saved.length - 40];
        System.arraycopy(saved, 0, truncated, 0, truncated.length);
        assertRefusedOpenChangesNothing(truncated, R.string.status_project_damaged);
    }

    @Test
    public void e2er1a05_anUnsupportedMajorIsRefusedAndChangesNothing() {
        final byte[] saved = saveAndRead();
        final byte[] newer = saved.clone();
        newer[8] = 2;  // the major, little-endian
        assertRefusedOpenChangesNothing(newer, R.string.status_project_unsupported);
    }

    @Test
    public void e2er1a04_bytesThatAreNotAProjectAreRefusedAndChangeNothing() {
        saveAndRead();
        final byte[] rubbish = new byte[256];
        for (int i = 0; i < rubbish.length; i++) {
            rubbish[i] = (byte) (i * 7 + 3);
        }
        assertRefusedOpenChangesNothing(rubbish, R.string.status_project_not_a_project);
    }

    /**
     * Injects bytes into the slot, presses Open, and proves that the refusal was
     * explicit, non-crashing, and total: not one value below JNI moved.
     */
    private void assertRefusedOpenChangesNothing(byte[] injected, int expectedMessage) {
        assertTrue("the test must be able to inject into the app-private slot",
                ProjectSlot.write(context(), injected));

        final double[] before = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());
        final long activeBefore = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneActiveBodyId());
        final int bodiesBefore = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneBodyCount());
        final int undoBefore = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.constructionUndoDepth());

        openProjectSurface();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openSavedProjectDiscardingChanges(workspace);
            return null;
        });
        settleLayout();

        final double[] after = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());
        assertArrayEquals("a refused Open must not move one native value:"
                        + describeSnapshotDifference(before, after), before, after, 0.0);
        assertEquals("a refused Open must not change the active body", activeBefore,
                (long) onWorkspace(rule.getScenario(),
                        (activity, workspace) -> NativeViewport.sceneActiveBodyId()));
        assertEquals("a refused Open must not change the scene", bodiesBefore,
                (int) onWorkspace(rule.getScenario(),
                        (activity, workspace) -> NativeViewport.sceneBodyCount()));
        assertEquals("a refused Open must not clear the session history", undoBefore,
                (int) onWorkspace(rule.getScenario(),
                        (activity, workspace) -> NativeViewport.constructionUndoDepth()));

        final String expected = context().getString(expectedMessage);
        final CharSequence reported = onWorkspace(rule.getScenario(),
                (activity, workspace) ->
                        ((android.widget.TextView) workspace.findViewById(
                                R.id.status_message)).getText());
        assertEquals("the refusal must say what happened and that the work is safe",
                expected, String.valueOf(reported));
    }

    // -----------------------------------------------------------------------
    // E2ER1A-06: a successful load starts a fresh session history
    // -----------------------------------------------------------------------

    @Test
    public void e2er1a06_aSuccessfulOpenClearsUndoAndTheNextEditWorks() {
        saveAndRead();

        // Two real Construction edits AFTER the save, so there is a history to
        // lose and the loaded document is genuinely a different state.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionSphere(3.25);
            NativeViewport.applyConstructionSphere(4.5);
            return null;
        });
        settleLayout();
        assertTrue("precondition: there must be a history to clear",
                onWorkspace(rule.getScenario(),
                        (activity, workspace) -> NativeViewport.constructionUndoDepth()) > 0);

        openProjectSurface();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openSavedProjectDiscardingChanges(workspace);
            return null;
        });
        settleLayout();

        assertEquals("a successful load starts a fresh session: no undo steps", 0,
                (int) onWorkspace(rule.getScenario(),
                        (activity, workspace) -> NativeViewport.constructionUndoDepth()));
        assertEquals("...and no redo steps either", 0,
                (int) onWorkspace(rule.getScenario(),
                        (activity, workspace) -> NativeViewport.constructionRedoDepth()));
        assertTrue("the loaded state is the saved one, not the two later edits",
                Math.abs(loadedBoxWidth() - WorkspaceTestSupport.BASELINE_WIDTH_METERS) < 1e-9);

        // A new edit over the loaded scene records exactly one step, and undoing
        // it returns the LOADED value — proof that the fresh history refers to
        // the loaded scene and not to the one it replaced.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionBox(5.0, 1.0, 0.5);
            return null;
        });
        assertEquals("a post-load edit records exactly one step", 1,
                (int) onWorkspace(rule.getScenario(),
                        (activity, workspace) -> NativeViewport.constructionUndoDepth()));
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.HISTORY_OK, NativeViewport.constructionUndo());
            return null;
        });
        assertTrue("a post-load undo returns the LOADED value",
                Math.abs(loadedBoxWidth() - WorkspaceTestSupport.BASELINE_WIDTH_METERS) < 1e-9);
    }

    // -----------------------------------------------------------------------
    // Shared machinery
    // -----------------------------------------------------------------------

    /** Saves through the real control and hands back the bytes that landed. */
    private byte[] saveAndRead() {
        openProjectSurface();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.project_save).performClick();
            return null;
        });
        settleLayout();
        final byte[] bytes = ProjectSlot.read(context());
        assertNotNull("the save must have written the slot", bytes);
        assertTrue("the save must have written a whole file", bytes.length > 64);
        return bytes;
    }

    /** Opens the project surface from the control a user would press. */
    private void openProjectSurface() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            if (!workspace.projectPopover().isOpen()) {
                workspace.findViewById(R.id.project_actions_button).performClick();
            }
            return null;
        });
        settleLayout();
    }

    private double loadedBoxWidth() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] state = new double[NativeViewport.PRIMITIVE_STATE_SIZE];
            NativeViewport.constructionPrimitive(state);
            return state[NativeViewport.PRIMITIVE_BOX_WIDTH];
        });
    }

    private Rect trailingHostBounds() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View host = WorkspaceTestSupport.trailingHost(workspace);
            final int[] location = new int[2];
            host.getLocationInWindow(location);
            return new Rect(location[0], location[1], location[0] + host.getWidth(),
                    location[1] + host.getHeight());
        });
    }
}
