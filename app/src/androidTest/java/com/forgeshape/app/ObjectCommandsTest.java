package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * `E2E-OBJ018A-01`: the one device journey for Stage 018A's object commands.
 *
 * <h2>What this suite is for, and what it deliberately is not</h2>
 *
 * <p>The DOMAIN half — that each command is exactly one history transaction,
 * that Undo and Redo are exact, that a hidden body leaves the snapshot the
 * renderer and the picker share, that a duplicate gets a fresh ObjectId and
 * clones the source's truth without its sculpt Undo stack, that a
 * face-supported CAD body is refused by name, and that visibility, lock and
 * name round-trip through `.forge` while an older file still loads visible and
 * unlocked — is proved by the scene and CAD self-tests (`OBJ018A-01..14`),
 * which build their own scenes and cost milliseconds.
 *
 * <p>What is left, and all this covers, is the single journey that can only be
 * true on a device: a user opens the Objects panel, reaches the row's overflow,
 * and drives all four commands through the real controls — with the transform
 * gizmo actually withdrawn while the body is locked and actually restored when
 * it is unlocked. It is ONE flow over ONE representation on purpose: repeating
 * it per representation would re-prove the representation-neutrality the domain
 * suites already establish, and Stage 018A is running under a reduced-testing
 * policy (`TEST-OWNER-03`).
 */
@RunWith(AndroidJUnit4.class)
public final class ObjectCommandsTest {

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private byte[] baselineProject;

    @Before
    public void setUp() {
        resetToBaselineConstruction(rule.getScenario());
        baselineProject = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.encodeProject());
    }

    @After
    public void tearDown() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.enterConstructionMode();
            if (baselineProject != null) {
                NativeViewport.loadProject(baselineProject);
            }
            return null;
        });
        resetToBaselineConstruction(rule.getScenario());
    }

    /**
     * Rename, Hide, Show, Lock, Unlock and Duplicate, driven through the real
     * row controls, in one pass over a two-body project.
     */
    @Test
    public void e2eObj018a01_theRowCommandsRunThroughTheRealControls() {
        final long[] ids = addBodies(1);
        final long target = ids[ids.length - 1];
        assertTrue("the journey needs at least two bodies", ids.length >= 2);

        openObjects();
        openRowCommands(target);

        // --- Rename -------------------------------------------------------
        final int undoBeforeRename = undoDepth();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View rename = find(workspace, R.id.object_row_rename, target);
            assertNotNull("the strip offers Rename", rename);
            assertMeetsTouchFloor(activity, rename, "Rename");
            rename.performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final android.widget.EditText field =
                    (android.widget.EditText) workspace.findViewById(R.id.object_rename_field);
            assertNotNull("Rename opens an inline field", field);
            field.setText("carrier plate");
            workspace.findViewById(R.id.object_rename_commit).performClick();
            return null;
        });
        settleLayout();
        assertEquals("the rename reached the domain", "carrier plate", nameOf(target));
        assertEquals("and it is exactly one history step", undoBeforeRename + 1, undoDepth());
        // The label the user reads follows the stored name, in the one place
        // that answers for every surface.
        assertEquals("the row label reads the new name", "carrier plate", rowLabel(target));

        // --- Hide, then Show ----------------------------------------------
        openRowCommands(target);
        final int undoBeforeHide = undoDepth();
        clickRowControl(target, R.id.object_row_visibility, "Show/Hide");
        assertFalse("the body is hidden", visible(target));
        assertEquals("hiding is one history step", undoBeforeHide + 1, undoDepth());
        assertNotNull("but the row is still there", rowFor(target));

        openRowCommands(target);
        clickRowControl(target, R.id.object_row_visibility, "Show/Hide");
        assertTrue("and showing it again brings it back", visible(target));

        // --- Lock: the gizmo must actually go ------------------------------
        select(target);
        enterTransform();
        assertTrue("the transform gizmo is offered before the lock", gizmoActive());

        openRowCommands(target);
        final int undoBeforeLock = undoDepth();
        clickRowControl(target, R.id.object_row_lock, "Lock/Unlock");
        assertTrue("the body is locked", locked(target));
        assertEquals("locking is one history step", undoBeforeLock + 1, undoDepth());
        assertFalse("and the transform gizmo is withdrawn over a locked body", gizmoActive());
        // The guard is below JNI as well: removing a control is not removing a
        // guard, so the domain refuses the write even when it is reached
        // directly.
        assertEquals("a transform write against a locked body is rejected",
                NativeViewport.APPLY_REJECTED_LOCKED, applyCurrentTransform());

        openRowCommands(target);
        clickRowControl(target, R.id.object_row_lock, "Lock/Unlock");
        assertFalse("unlocking restores the movable state", locked(target));
        enterTransform();
        assertTrue("and the gizmo comes back", gizmoActive());

        // --- Duplicate ------------------------------------------------------
        final long[] before = sceneBodyIds();
        final int undoBeforeDuplicate = undoDepth();
        openRowCommands(target);
        clickRowControl(target, R.id.object_row_duplicate, "Duplicate");

        final long[] after = sceneBodyIds();
        assertEquals("the duplicate added exactly one body", before.length + 1, after.length);
        assertEquals("and it is one history step", undoBeforeDuplicate + 1, undoDepth());
        final long copy = activeBodyId();
        assertFalse("the copy wears a NEW ObjectId", contains(before, copy));
        assertEquals("and the copy is the active body", copy, activeBodyId());
        assertNotEquals("which is not the source", target, copy);
        assertEquals("the copy carries a deterministic copy name",
                "carrier plate copy", nameOf(copy));
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final ObjectsSectionView objects = objectsSection(workspace);
            assertEquals("the list grew by one row", after.length, objects.rowCount());
            assertNotNull("and the copy has a row of its own", objects.rowFor(copy));
            return null;
        });
        closeObjects();
    }

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    /**
     * Ensures one row's command strip is open, through its overflow.
     *
     * <p>Idempotent on purpose: the overflow TOGGLES, and a toggle command such
     * as Show/Hide deliberately leaves the strip standing so the user can act
     * again. Clicking unconditionally would therefore close the strip the
     * second time this is called, which is a property of the control rather
     * than a defect — so the helper asks whether the row is already open first.
     */
    private void openRowCommands(final long objectId) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            if (Long.valueOf(objectId).equals(objectsSection(workspace).expandedRow())) {
                return null;  // already standing; a click here would close it
            }
            final View more = find(workspace, R.id.object_row_more, objectId);
            assertNotNull("the row offers its overflow", more);
            assertMeetsTouchFloor(activity, more, "the row overflow");
            more.performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("the strip is open for the row that was asked for",
                    Long.valueOf(objectId), objectsSection(workspace).expandedRow());
            return null;
        });
    }

    private void clickRowControl(final long objectId, final int id, final String what) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View control = find(workspace, id, objectId);
            assertNotNull(what + " is offered in the strip", control);
            assertMeetsTouchFloor(activity, control, what);
            assertNotNull(what + " names what it acts on", control.getContentDescription());
            control.performClick();
            return null;
        });
        settleLayout();
    }

    /**
     * A control inside the Objects section, found by its semantic id and its
     * ObjectId tag — never by position on screen.
     */
    private static View find(EditorWorkspaceView workspace, int id, long objectId) {
        // Walked rather than `findViewById`, because every row carries the same
        // ids: which body a control means is its TAG, exactly as the row label's
        // is. `findViewById` would answer with whichever row happens to be first.
        return search(objectsSection(workspace), id, Long.valueOf(objectId));
    }

    private static View search(View view, int id, Long objectId) {
        if (view.getId() == id && objectId.equals(view.getTag())) {
            return view;
        }
        if (view instanceof android.view.ViewGroup) {
            final android.view.ViewGroup group = (android.view.ViewGroup) view;
            for (int i = 0; i < group.getChildCount(); i++) {
                final View found = search(group.getChildAt(i), id, objectId);
                if (found != null) {
                    return found;
                }
            }
        }
        return null;
    }

    /** 48 dp is the interactive floor, and it is the HIT AREA. */
    private static void assertMeetsTouchFloor(android.app.Activity activity, View view,
                                              String what) {
        final float density = activity.getResources().getDisplayMetrics().density;
        final int floor = Math.round(48f * density);
        assertTrue(what + " meets the 48 dp floor: " + view.getWidth() + "x" + view.getHeight(),
                view.getWidth() >= floor - 1 && view.getHeight() >= floor - 1);
    }

    private long[] addBodies(int count) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            for (int i = 0; i < count; i++) {
                assertNotEquals("a body must be created", 0L, NativeViewport.sceneAddBody());
            }
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        return sceneBodyIds();
    }

    private void openObjects() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openObjectsPanel(workspace);
            return null;
        });
        settleLayout();
    }

    private void closeObjects() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.closeObjectsPanel(workspace);
            return null;
        });
        settleLayout();
    }

    private void select(final long objectId) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.sceneSelectBody(objectId);
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    /** Puts the workspace into Transform, which is what offers the gizmo. */
    private void enterTransform() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.uiState().setConstructionTool(EditorUiState.CONSTRUCTION_TOOL_TRANSFORM);
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    private boolean gizmoActive() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] state = new double[12];
            NativeViewport.gizmoState(state);
            return state[NativeViewport.GIZMO_ACTIVE] != 0.0;
        });
    }

    /** Re-applies the body's CURRENT placement, which a lock must still reject. */
    private int applyCurrentTransform() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] t = new double[9];
            NativeViewport.boxTransform(t);
            return NativeViewport.applyBoxTransform(t[0], t[1], t[2], t[3], t[4], t[5],
                    t[6], t[7], t[8]);
        });
    }

    private String nameOf(final long objectId) {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneBodyName(objectId));
    }

    private String rowLabel(final long objectId) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View row = objectsSection(workspace).rowFor(objectId);
            assertNotNull("the body has a row", row);
            return ((android.widget.TextView) row).getText().toString();
        });
    }

    private View rowFor(final long objectId) {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> objectsSection(workspace).rowFor(objectId));
    }

    private boolean visible(final long objectId) {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneBodyVisible(objectId));
    }

    private boolean locked(final long objectId) {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneBodyLocked(objectId));
    }

    private int undoDepth() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.constructionUndoDepth());
    }

    private long activeBodyId() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneActiveBodyId());
    }

    private long[] sceneBodyIds() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final long[] buffer = new long[NativeViewport.sceneBodyCount()];
            final int written = NativeViewport.sceneBodyIds(buffer);
            final long[] ids = new long[written];
            System.arraycopy(buffer, 0, ids, 0, written);
            return ids;
        });
    }

    private static ObjectsSectionView objectsSection(EditorWorkspaceView workspace) {
        final View view = workspace.findViewById(R.id.objects_section);
        assertNotNull("the Objects section is on screen", view);
        return (ObjectsSectionView) view;
    }

    private static boolean contains(long[] values, long wanted) {
        for (long value : values) {
            if (value == wanted) {
                return true;
            }
        }
        return false;
    }
}
