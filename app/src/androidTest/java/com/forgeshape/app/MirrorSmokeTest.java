package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
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
 * `E2E-MIRROR01-01`: the one device journey for Construction Mirror.
 *
 * <h2>What this suite is for, and what it deliberately is not</h2>
 *
 * <p>The DOMAIN half — that the position reflects across the chosen world
 * plane, that the mirrored rotation is proper and the Absolute Scale untouched,
 * that the mirrored world geometry IS the reflection of the source's for every
 * primitive on every plane, that one Mirror is one history step, that Undo
 * removes only the reflection and Redo restores the same ObjectId, that an
 * Imported Mesh, a CAD Body and a body carrying sculpt truth are refused by
 * name, and that the result round-trips through the ordinary `.forge` codec
 * with no schema change — is proved by `MIRROR01-01..12`, which build their own
 * scenes and cost milliseconds.
 *
 * <p>What is left, and all this covers, is the single journey that can only be
 * true on a device: a user moves and turns an object, opens the Objects panel,
 * reaches the row's overflow, chooses Mirror, picks a plane in the compact
 * chooser, and gets a second selected object — then takes it back and puts it
 * again. ONE plane and ONE primitive on purpose: repeating it per plane would
 * re-prove arithmetic the domain suite already establishes over all three, and
 * `MIRROR-01` runs under the reduced-testing policy (`TEST-OWNER-03`).
 */
@RunWith(AndroidJUnit4.class)
public final class MirrorSmokeTest {

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

    @Test
    public void e2eMirror0101_theRowMirrorsThroughTheRealControls() {
        final long source = activeBodyId();
        assertNotEquals("the journey needs a body", 0L, source);

        // Moved off every plane and turned about all three axes, so a
        // reflection that only negated a coordinate would be visibly wrong in
        // the numbers themselves.
        final double[] start = {2.5, -1.5, 0.75, 31.0, -47.5, 118.25, 1.5, 2.5, 0.5};
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("the placement is accepted", 0,
                    NativeViewport.applyBoxTransform(start[0], start[1], start[2], start[3],
                            start[4], start[5], start[6], start[7], start[8]));
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();

        openObjects();
        openRowCommands(source);

        // --- the chooser is a CHOICE, not yet an act ----------------------
        final int undoBefore = undoDepth();
        final long[] before = sceneBodyIds();
        clickRowControl(source, R.id.object_row_mirror, "Mirror");
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertNotNull("Mirror opens the compact plane chooser",
                    find(workspace, R.id.object_mirror_planes, source));
            final int[] planes = {R.id.object_mirror_plane_xy, R.id.object_mirror_plane_xz,
                                  R.id.object_mirror_plane_yz};
            for (int id : planes) {
                final View plane = find(workspace, id, source);
                assertNotNull("the chooser offers all three planes", plane);
                assertMeetsTouchFloor(activity, plane, "a plane chip");
                assertNotNull("and each names the axis it reflects",
                        plane.getContentDescription());
            }
            return null;
        });
        assertEquals("opening the chooser creates nothing", before.length, sceneBodyIds().length);
        assertEquals("and records no history step", undoBefore, undoDepth());

        // --- choosing YZ commits ------------------------------------------
        clickRowControl(source, R.id.object_mirror_plane_yz, "the YZ plane");

        final long[] after = sceneBodyIds();
        assertEquals("the mirror added exactly one body", before.length + 1, after.length);
        assertEquals("and it is exactly one history step", undoBefore + 1, undoDepth());
        final long reflection = activeBodyId();
        assertFalse("the reflection wears a NEW ObjectId", contains(before, reflection));
        assertNotEquals("which is not the source", source, reflection);

        // The SOURCE is untouched, and the reflection wears the reflected X.
        final double[] sourceNow = transformOf(source);
        for (int i = 0; i < 9; i++) {
            assertEquals("the source is unchanged, value " + i, start[i], sourceNow[i], 0.0);
        }
        final double[] mirrored = transformOf(reflection);
        assertEquals("YZ reflects world X", -start[0], mirrored[0], 0.0);
        assertEquals("and leaves Y alone", start[1], mirrored[1], 0.0);
        assertEquals("and leaves Z alone", start[2], mirrored[2], 0.0);
        assertEquals("the Absolute Scale is carried across, X", start[6], mirrored[6], 0.0);
        assertEquals("the Absolute Scale is carried across, Y", start[7], mirrored[7], 0.0);
        assertEquals("the Absolute Scale is carried across, Z", start[8], mirrored[8], 0.0);
        assertTrue("and stays strictly positive",
                mirrored[6] > 0.0 && mirrored[7] > 0.0 && mirrored[8] > 0.0);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final ObjectsSectionView objects = objectsSection(workspace);
            assertEquals("the list grew by one row", after.length, objects.rowCount());
            assertNotNull("and the reflection has a row of its own", objects.rowFor(reflection));
            assertNull("the chooser closed on the commit", objects.expandedRow());
            return null;
        });

        // --- Undo removes only the reflection; Redo puts the same one back --
        undo();
        assertEquals("Undo removes exactly the reflection", before.length,
                sceneBodyIds().length);
        assertFalse("and it is gone from the scene", contains(sceneBodyIds(), reflection));
        assertEquals("the previous selection is restored", source, activeBodyId());

        redo();
        assertEquals("Redo puts one body back", after.length, sceneBodyIds().length);
        assertTrue("and it is the SAME ObjectId", contains(sceneBodyIds(), reflection));
        assertEquals("wearing the same reflected placement", -start[0],
                transformOf(reflection)[0], 0.0);
        assertEquals("and it is the active body again", reflection, activeBodyId());

        closeObjects();
    }

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    private void openRowCommands(final long objectId) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            if (Long.valueOf(objectId).equals(objectsSection(workspace).expandedRow())) {
                return null;  // already standing; a click here would close it
            }
            final View more = find(workspace, R.id.object_row_more, objectId);
            assertNotNull("the row offers its overflow", more);
            more.performClick();
            return null;
        });
        settleLayout();
    }

    private void clickRowControl(final long objectId, final int id, final String what) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View control = find(workspace, id, objectId);
            assertNotNull(what + " is offered", control);
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
        // ids: which body a control means is its TAG, exactly as the row
        // label's is.
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

    private void undo() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("Construction Undo is performed", 0, NativeViewport.constructionUndo());
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    private void redo() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("Construction Redo is performed", 0, NativeViewport.constructionRedo());
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    /**
     * The nine authoritative values of one body's placement.
     *
     * <p>Read through the active body, which is the one entry point the
     * exact-value editors use, and the previous selection is put back
     * afterwards so reading a value cannot change what the user had chosen.
     */
    private double[] transformOf(final long objectId) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final long previous = NativeViewport.sceneActiveBodyId();
            NativeViewport.sceneSelectBody(objectId);
            final double[] values = new double[9];
            NativeViewport.boxTransform(values);
            NativeViewport.sceneSelectBody(previous);
            return values;
        });
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
