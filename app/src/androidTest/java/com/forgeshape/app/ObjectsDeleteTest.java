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
 * `IMP01B-15..24` and `E2E-IMP01B-08..11`: Delete removes a real project object.
 *
 * <h2>What this suite is for</h2>
 *
 * <p>The DOMAIN half — that one Delete is one transaction, that Undo restores
 * the SAME object with its Imported Mesh and its Frozen Sculpt Mesh intact, that
 * the replacement selection is deterministic and that the last body is refused
 * by name — is proved by the Construction-history self-tests, which build their
 * own scenes. What is left, and what this covers, is everything that can only be
 * true on a device: the row's control and its geometry, that a deleted body
 * stops rendering, picking, saving, exporting and recovering, and that the
 * chrome afterwards describes the body that replaced it.
 *
 * <h2>Scope that must stay absent</h2>
 *
 * <p>`UI-OWNER-45` is Delete and nothing else. Rename, visibility, lock,
 * duplicate and grouping are Stage 018A's and are asserted absent here.
 */
@RunWith(AndroidJUnit4.class)
public final class ObjectsDeleteTest {

    private static final String SIX_NODE_GLB = "glb/construction_sentinel.glb";

    /** How long the autosave worker gets to drain. Never slept for; see awaitIdle. */
    private static final long IDLE_TIMEOUT_MS = 5000L;

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private final File scratch = new File(context().getCacheDir(), "objects-delete-test");

    private static Context context() {
        return InstrumentationRegistry.getInstrumentation().getTargetContext();
    }

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
    // IMP01B-15/23 / E2E-IMP01B-08: the control, and what pressing it does
    // -----------------------------------------------------------------------

    /**
     * The row's Delete removes the actual body, as exactly one history step.
     *
     * <p>The control is found by its semantic id and its ObjectId tag, never by
     * position on screen, and it is a SIBLING of the label rather than the label
     * itself — so the tap that chooses a body and the tap that removes it are
     * two different targets.
     */
    @Test
    public void imp01b15and23_theRowsDeleteRemovesTheBodyAsOneHistoryStep() {
        final long[] ids = addBodies(2);
        final long doomed = ids[ids.length - 1];
        final long keep = ids[0];
        final int undoBefore = undoDepth();

        openObjects();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final ObjectsSectionView objects = objectsSection(workspace);
            final View label = objects.rowFor(doomed);
            final View delete = objects.deleteControlFor(doomed);
            assertNotNull("IMP01B-23: the row has a Delete control", delete);
            assertNotEquals("IMP01B-23: and it is not the row label itself", label, delete);
            assertEquals("IMP01B-23: it carries a stable semantic id",
                    R.id.object_row_delete, delete.getId());
            assertEquals("IMP01B-23: and names the body it means",
                    activity.getString(R.string.delete_body, ((TextView) label).getText()),
                    delete.getContentDescription().toString());

            // IMP01B-23: the 48 dp interactive floor, in BOTH dimensions, and it
            // is the HIT AREA -- the glyph inside it is still 20 dp.
            final float density = activity.getResources().getDisplayMetrics().density;
            final int floor = Math.round(48f * density);
            assertTrue("IMP01B-23: the Delete control meets the 48 dp floor: "
                            + delete.getWidth() + "x" + delete.getHeight(),
                    delete.getWidth() >= floor - 1 && delete.getHeight() >= floor - 1);
            assertTrue("IMP01B-23: and the row label is still reachable beside it",
                    label.getWidth() > 0 && label.getHeight() >= floor - 1);

            delete.performClick();
            return null;
        });
        settleLayout();

        final long[] after = sceneBodyIds();
        assertEquals("IMP01B-15: the body is gone from the scene", ids.length - 1, after.length);
        for (long id : after) {
            assertNotEquals("and it is the one that was asked for", doomed, id);
        }
        assertEquals("IMP01B-15: one Delete is exactly one history step",
                undoBefore + 1, undoDepth());
        assertEquals("IMP01B-20: the selection landed on a body that still exists",
                true, contains(after, activeBodyId()));

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final ObjectsSectionView objects = objectsSection(workspace);
            assertEquals("E2E-IMP01B-08: the row disappeared with the body",
                    after.length, objects.rowCount());
            assertNull("and it cannot be found by tag any more", objects.rowFor(doomed));
            assertNotNull("while the body beside it still has one", objects.rowFor(keep));
            return null;
        });
        closeObjects();
    }

    // -----------------------------------------------------------------------
    // IMP01B-18/19 / E2E-IMP01B-09: Undo restores, Redo removes again
    // -----------------------------------------------------------------------

    @Test
    public void imp01b18and19_undoRestoresTheExactBodyAndRedoRemovesItAgain() {
        final long[] ids = addBodies(2);
        final long doomed = ids[ids.length - 1];
        final byte[] projectBefore = encodeProject();

        deleteThroughTheRow(doomed);
        final byte[] projectAfterDelete = encodeProject();
        assertNotEquals("a delete is a real project change",
                describeBytes(projectBefore), describeBytes(projectAfterDelete));

        undo();
        assertArrayEquals("IMP01B-18: Undo restores the body, in its own place",
                projectBefore, encodeProject());
        assertTrue("IMP01B-18: and the row comes back with it", rowExists(doomed));

        redo();
        assertArrayEquals("IMP01B-19: Redo removes the same body again",
                projectAfterDelete, encodeProject());
        assertFalse("IMP01B-19: and the row goes with it", rowExists(doomed));
    }

    /**
     * IMP01B-16/17/18. An Imported Mesh carrying retained sculpt work deletes
     * and comes back whole.
     *
     * <p>This is the case a per-representation delete path would have broken:
     * neither an imported object's geometry nor a Frozen Sculpt Mesh is derived
     * from anything a history step holds, so an Undo that rebuilt the body
     * rather than restoring it would come back with an empty object wearing the
     * right identity. The whole `.forge` document is compared, which is where
     * both of those live.
     */
    @Test
    public void imp01b16and17_anImportedBodyWithSculptWorkDeletesAndReturnsWhole() {
        final long imported = importOneBodyAndSculptIt();
        final byte[] projectBefore = encodeProject();
        assertTrue("precondition: the project carries both branches for that body",
                sectionPresent(projectBefore, "IMPT") && sectionPresent(projectBefore, "SCUL"));

        deleteThroughTheRow(imported);

        final byte[] afterDelete = encodeProject();
        // "No orphan record survives" is asked of the DOCUMENT's own rule
        // rather than of whether a section exists at all.
        //
        // Asserting that `IMPT` or `SCUL` is absent entirely would be asserting
        // something about every OTHER body in the scene, and the scene is
        // process-scoped: another case in the same shard may legitimately have
        // left an imported body or a sculpt mesh behind. `validateProjectDocument`
        // refuses a `CONS`, `IMPT` or `SCUL` entry naming a body `SCNE` does not
        // carry (`UnresolvedReference`), so a document that still passes it
        // provably carries no orphan record for ANY body — which is the rule
        // IMP01B-17 is about, stated more strongly than a section check could.
        assertEquals("IMP01B-17: the document carries no orphan CONS, IMPT or SCUL record",
                NativeViewport.PROJECT_OK, validateProject(afterDelete));
        assertFalse("IMP01B-17: and the deleted body is gone from the scene",
                contains(sceneBodyIds(), imported));
        assertNotEquals("IMP01B-17: its geometry and its sculpt mesh really left the document",
                describeBytes(projectBefore), describeBytes(afterDelete));
        assertTrue("IMP01B-17: the document got smaller by more than a body record",
                afterDelete.length < projectBefore.length);
        undo();
        assertArrayEquals("IMP01B-18: the imported geometry AND the sculpt mesh came back",
                projectBefore, encodeProject());
    }

    // -----------------------------------------------------------------------
    // IMP01B-20 / E2E-IMP01B-10: the replacement selection, and the chrome
    // -----------------------------------------------------------------------

    /**
     * Deleting the ACTIVE body selects the next row, and the chrome then
     * describes the body that replaced it.
     *
     * <p>The second half is what makes the first safe: a stale <i>Resume
     * Sculpt</i> left over from the deleted body would offer to return to a mesh
     * that no longer exists.
     */
    @Test
    public void imp01b20_deletingTheActiveBodySelectsTheNextRowAndTheChromeFollows() {
        final long[] ids = addBodies(3);
        final long first = ids[ids.length - 3];
        final long middle = ids[ids.length - 2];
        final long last = ids[ids.length - 1];

        // A retained sculpt mesh on the body about to be deleted, so the chrome
        // it leaves behind would be visibly wrong if it were not re-read.
        sculptThenLeave(middle);
        select(middle);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("precondition: the doomed body offers Resume Sculpt",
                    View.VISIBLE, workspace.findViewById(R.id.resume_sculpt).getVisibility());
            return null;
        });

        deleteThroughTheRow(middle);

        assertEquals("IMP01B-20: the selection fell to the NEXT row", last, activeBodyId());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("IMP01B-20: and no stale Resume Sculpt is left from the deleted body",
                    View.GONE, workspace.findViewById(R.id.resume_sculpt).getVisibility());
            assertEquals("the replacement's own transition is offered instead",
                    View.VISIBLE,
                    workspace.findViewById(R.id.freeze_to_sculpt).getVisibility());
            return null;
        });

        // And the other half of the rule: deleting the LAST row falls back to
        // the one before it.
        select(last);
        deleteThroughTheRow(last);
        assertEquals("IMP01B-20: deleting the last row selects the one before it",
                previousOf(sceneBodyIds(), last, first), activeBodyId());

        // Deleting a body that is NOT active leaves the selection alone.
        final long active = activeBodyId();
        final long other = otherThan(sceneBodyIds(), active);
        deleteThroughTheRow(other);
        assertEquals("IMP01B-20: deleting an inactive body does not move the selection",
                active, activeBodyId());
    }

    // -----------------------------------------------------------------------
    // IMP01B-21: the last body, and the two withdrawals
    // -----------------------------------------------------------------------

    /**
     * The last body cannot be deleted, the control is not drawn for it, and the
     * domain refusal is named and costs nothing.
     *
     * <p>The project has no empty state — the scene creates a body eagerly,
     * every active-body accessor assumes one, and a `.forge` file with zero
     * bodies is refused — so this is the stable named refusal `UI-OWNER-45`
     * asks for rather than an empty scene nothing downstream could hold.
     */
    @Test
    public void imp01b21_theLastBodyIsRefusedByNameAndItsControlIsNotDrawn() {
        reduceToOneBody();

        openObjects();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final ObjectsSectionView objects = objectsSection(workspace);
            assertEquals("precondition: exactly one row", 1, objects.rowCount());
            assertNull("IMP01B-21: the only body's row draws no Delete",
                    objects.deleteControlFor(NativeViewport.sceneActiveBodyId()));
            return null;
        });
        closeObjects();

        // And the guard is still in the DOMAIN: withdrawing a control is not
        // removing its guard.
        final byte[] before = encodeProject();
        final int undoBefore = undoDepth();
        final long only = activeBodyId();
        assertEquals("IMP01B-21: the domain refuses the last body by name",
                NativeViewport.DELETE_REFUSED_LAST_BODY, deleteBelowJni(only));
        assertArrayEquals("IMP01B-21: and the refusal changed no project byte",
                before, encodeProject());
        assertEquals("IMP01B-21: and recorded no history step", undoBefore, undoDepth());
        assertEquals("and no replacement body was invented", 1, sceneBodyIds().length);
    }

    /**
     * IMP01B-21. Delete is refused while sculpting, and withdrawn there.
     *
     * <p>The same rule body switching and Undo/Redo already follow: the Sculpt
     * target is fixed for the duration of the mode, and Undo is refused there —
     * so a delete made in Sculpt could not be taken back until the user left it.
     */
    @Test
    public void imp01b21_deleteIsWithdrawnAndRefusedWhileSculpting() {
        final long[] ids = addBodies(2);
        final long target = ids[ids.length - 1];
        select(target);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.APPLY_APPLIED, NativeViewport.applyConstructionSphere(1.0));
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.freezeToSculpt());
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();

        openObjects();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final ObjectsSectionView objects = objectsSection(workspace);
            assertTrue("the scene is still worth seeing in Sculpt", objects.rowCount() > 1);
            assertNull("IMP01B-21: but no row draws a Delete while sculpting",
                    objects.deleteControlFor(target));
            assertFalse("and creation is still withdrawn there too",
                    objects.creationAvailable());
            return null;
        });
        closeObjects();

        final byte[] before = encodeProject();
        assertEquals("IMP01B-21: and the domain refuses it by name",
                NativeViewport.DELETE_REFUSED_IN_SCULPT, deleteBelowJni(target));
        assertArrayEquals("changing nothing", before, encodeProject());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.enterConstructionMode();
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        openObjects();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertNotNull("and it comes back the moment Sculpt is left",
                    objectsSection(workspace).deleteControlFor(target));
            return null;
        });
        closeObjects();
    }

    // -----------------------------------------------------------------------
    // IMP01B-22 / E2E-IMP01B-11: no ghost anywhere
    // -----------------------------------------------------------------------

    /**
     * A deleted body is absent from picking, from the saved document, from the
     * autosave checkpoint and from the exported GLB — and Undo brings it back to
     * all four.
     */
    @Test
    public void imp01b22_aDeletedBodyIsAbsentFromPickingSaveAutosaveAndExport() {
        final long[] ids = addBodies(2);
        final long doomed = ids[ids.length - 1];

        // Isolate the doomed body under the viewport centre so that picking has
        // something to say about it either way.
        isolateAtOrigin(doomed);
        assertEquals("precondition: the doomed body is what the centre picks",
                doomed, pickAtViewportCentre());

        final byte[] withIt = encodeProject();
        final byte[] exportWithIt = exportGlb();

        deleteThroughTheRow(doomed);

        assertNotEquals("IMP01B-22: the deleted body cannot be picked",
                doomed, pickAtViewportCentre());
        final byte[] withoutIt = encodeProject();
        assertNotEquals("IMP01B-22: it is absent from the saved document",
                describeBytes(withIt), describeBytes(withoutIt));
        assertEquals("and the document is still one this build can open",
                NativeViewport.PROJECT_OK, validateProject(withoutIt));

        awaitAutosaveIdle();
        assertTrue("a delete is a project change and is checkpointed",
                ProjectCheckpoint.exists(context()));
        assertArrayEquals("IMP01B-22: the checkpoint has it gone too",
                withoutIt, ProjectCheckpoint.read(context()));

        final byte[] exportWithoutIt = exportGlb();
        assertNotEquals("IMP01B-22: and the exported GLB no longer carries it",
                describeBytes(exportWithIt), describeBytes(exportWithoutIt));
        assertTrue("but a file is still written for what remains",
                exportWithoutIt != null && exportWithoutIt.length > 0);

        // E2E-IMP01B-11: Undo before saving restores it to all of them.
        undo();
        assertArrayEquals("Undo restores the body to the saved document",
                withIt, encodeProject());
        assertEquals("and to the export", describeBytes(exportWithIt),
                describeBytes(exportGlb()));
        awaitAutosaveIdle();
        assertArrayEquals("and to the checkpoint", withIt, ProjectCheckpoint.read(context()));

        // Redo then save/export omits it again.
        redo();
        assertArrayEquals(withoutIt, encodeProject());
        assertEquals(describeBytes(exportWithoutIt), describeBytes(exportGlb()));
    }

    // -----------------------------------------------------------------------
    // IMP01B-24: the forbidden scope stays absent
    // -----------------------------------------------------------------------

    @Test
    public void imp01b24_theObjectsListGainedDeleteAndNothingElse() {
        addBodies(2);
        openObjects();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final StringBuilder text = new StringBuilder();
            collectVisibleText(objectsSection(workspace), text);
            final String surface = text.toString().toLowerCase(java.util.Locale.US);
            for (String absent : new String[]{"rename", "duplicate", "hide", "lock", "group"}) {
                assertFalse("IMP01B-24: no " + absent + " control is drawn: " + surface,
                        surface.contains(absent));
            }
            // The list is still rows plus Add body plus, now, one Delete per
            // row -- no reorder handle, no nesting and no chevron.
            assertTrue("Add body is still the only other control",
                    objectsSection(workspace).creationAvailable());
            return null;
        });
        closeObjects();
    }

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    /** Appends `count` Construction Bodies and returns the scene's ids afterwards. */
    private long[] addBodies(int count) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            for (int i = 0; i < count; i++) {
                assertNotEquals("a body must be created", 0L, NativeViewport.sceneAddBody());
            }
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        final long[] ids = sceneBodyIds();
        assertTrue("the scene has enough bodies for this case", ids.length > count);
        return ids;
    }

    /** Deletes through the ROW's control, which is how a user does it. */
    private void deleteThroughTheRow(final long objectId) {
        openObjects();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View delete = objectsSection(workspace).deleteControlFor(objectId);
            assertNotNull("the row must offer a Delete for " + objectId, delete);
            delete.performClick();
            return null;
        });
        settleLayout();
        closeObjects();
    }

    private boolean rowExists(final long objectId) {
        openObjects();
        final boolean present = onWorkspace(rule.getScenario(),
                (activity, workspace) -> objectsSection(workspace).rowFor(objectId) != null);
        closeObjects();
        return present;
    }

    /** Freezes a body, makes a real stroke and returns to Construction. */
    private void sculptThenLeave(final long objectId) {
        isolateAtOrigin(objectId);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(objectId));
            assertEquals(NativeViewport.APPLY_APPLIED, NativeViewport.applyConstructionSphere(1.0));
            NativeViewport.setSculptBrush(300.0, 1.0);
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.freezeToSculpt());
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.enterConstructionMode();
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    /**
     * Imports the fixture, keeps exactly one imported body, and sculpts it.
     *
     * <p>The other imported objects are deleted through the domain operation —
     * used here to isolate, not asserted; the cases above are what assert it.
     */
    private long importOneBodyAndSculptIt() {
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
            assertEquals(NativeViewport.APPLY_APPLIED,
                    NativeViewport.applyBoxTransform(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0,
                            1.0));
            NativeViewport.setSculptBrush(300.0, 1.0);
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.freezeToSculpt());
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.enterConstructionMode();
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        return keep;
    }

    /** Deletes every body but one, through the domain operation. */
    private void reduceToOneBody() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final long[] ids = new long[NativeViewport.sceneBodyCount()];
            final int written = NativeViewport.sceneBodyIds(ids);
            for (int i = 1; i < written; i++) {
                assertEquals(NativeViewport.DELETE_OK, NativeViewport.sceneDeleteBody(ids[i]));
            }
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        assertEquals("exactly one body remains", 1, sceneBodyIds().length);
    }

    /**
     * Puts one body at the origin at unit scale and moves every other body far
     * away, so the viewport centre picks exactly this one.
     */
    private void isolateAtOrigin(final long objectId) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final long[] ids = new long[NativeViewport.sceneBodyCount()];
            final int written = NativeViewport.sceneBodyIds(ids);
            double offset = 40.0;
            for (int i = 0; i < written; i++) {
                NativeViewport.sceneSelectBody(ids[i]);
                if (ids[i] == objectId) {
                    NativeViewport.applyBoxTransform(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
                } else {
                    NativeViewport.applyBoxTransform(offset, 0.0, 0.0, 0.0, 0.0, 0.0,
                            1.0, 1.0, 1.0);
                    offset += 40.0;
                }
            }
            NativeViewport.sceneSelectBody(objectId);
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    /**
     * Taps the viewport centre and returns whichever body ends up active.
     *
     * <p>The coordinate is the existing touch harness's, and nothing is inferred
     * FROM it: what is asserted is the ObjectId native code reports afterwards.
     */
    private long pickAtViewportCentre() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final int w = viewport.getWidth();
            final int h = viewport.getHeight();
            assertTrue("precondition: the viewport must be laid out", w > 0 && h > 0);
            sendViewportTouch(android.view.MotionEvent.ACTION_DOWN, w / 2f, h / 2f, w, h);
            sendViewportTouch(android.view.MotionEvent.ACTION_UP, w / 2f, h / 2f, w, h);
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        return activeBodyId();
    }

    private static void sendViewportTouch(int action, float x, float y, int width, int height) {
        NativeViewport.touchEvent(action, -1, 1, new int[]{0}, new float[]{x},
                new float[]{y}, null, null, null, null, width, height);
    }

    private void select(final long objectId) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(objectId));
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    private void undo() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.HISTORY_OK, NativeViewport.constructionUndo());
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    private void redo() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.HISTORY_OK, NativeViewport.constructionRedo());
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
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

    private static long previousOf(long[] remaining, long removed, long fallback) {
        // The body that sat before `removed` is whichever of the survivors is
        // last, because `removed` was last.
        return remaining.length > 0 ? remaining[remaining.length - 1] : fallback;
    }

    private static long otherThan(long[] ids, long active) {
        for (long id : ids) {
            if (id != active) {
                return id;
            }
        }
        throw new AssertionError("the case needs at least two bodies");
    }

    private long activeBodyId() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneActiveBodyId());
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

    private byte[] exportGlb() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.exportGlb());
    }

    private int validateProject(byte[] bytes) {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.validateProject(bytes));
    }

    /** The domain operation, reached directly: the guard is below JNI, not in a
     *  control that happens not to be drawn. */
    private int deleteBelowJni(final long objectId) {
        final Integer status = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneDeleteBody(objectId));
        return status.intValue();
    }

    private int undoDepth() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.constructionUndoDepth());
    }

    private void awaitAutosaveIdle() {
        assertTrue("the autosave worker must drain within the timeout",
                onWorkspace(rule.getScenario(), (activity, workspace) ->
                        workspace.autosaveController().awaitIdle(IDLE_TIMEOUT_MS)));
    }

    private static boolean sectionPresent(byte[] file, String tag) {
        if (file == null || file.length < 28) {
            return false;
        }
        int at = 28;
        while (at + 24 <= file.length) {
            final boolean matches = file[at] == tag.charAt(0) && file[at + 1] == tag.charAt(1)
                    && file[at + 2] == tag.charAt(2) && file[at + 3] == tag.charAt(3);
            long payloadBytes = 0;
            for (int i = 7; i >= 0; i--) {
                payloadBytes = (payloadBytes << 8) | (file[at + 8 + i] & 0xFFL);
            }
            final int end = at + 24 + (int) payloadBytes;
            if (payloadBytes < 0 || end > file.length) {
                return false;
            }
            if (matches) {
                return true;
            }
            at = end;
        }
        return false;
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

    private static void collectVisibleText(View view, StringBuilder out) {
        if (view == null || view.getVisibility() != View.VISIBLE) {
            return;
        }
        if (view instanceof TextView) {
            out.append(((TextView) view).getText()).append('\n');
        }
        if (view.getContentDescription() != null) {
            out.append(view.getContentDescription()).append('\n');
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
        try (InputStream in = InstrumentationRegistry.getInstrumentation()
                .getContext().getAssets().open(name)) {
            return drain(in);
        } catch (IOException e) {
            throw new AssertionError("asset " + name + " must be packaged", e);
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
