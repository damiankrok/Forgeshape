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

import android.content.Context;
import android.view.MotionEvent;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.FixMethodOrder;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.MethodSorters;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

/**
 * E2ER1A-02/03 and E2ER1B-01/02: ForgeShape work survives the process dying —
 * both the work the user saved, and the work they never did.
 *
 * <p>Stages 1-4 are the E2E-R1A claim: the user pressed Save, and Open brings
 * the project back. Stages 5-8 are the harder E2E-R1B one: the user pressed
 * nothing at all. Autosave alone protected the work, and on a genuinely cold
 * launch the recovery question is what offers it back — no seam, no cleared
 * flag, exactly what a user meets after a crash.
 *
 * <p><b>How the process death is real.</b> Instrumentation runs inside the app
 * process, so a test cannot kill that process and then keep asserting. The two
 * halves of each case are therefore separate test METHODS, and
 * {@code scripts\run-project-persistence-e2e.ps1} runs them as separate
 * {@code am instrument} invocations with an {@code am force-stop} in between —
 * so each verifying half genuinely starts in a process that has never seen the
 * scene its partner built. The
 * script confirms, by asking for the PID, that no ForgeShape process exists at
 * the moment the verifying half begins -- so everything that half reads about
 * the project came off the disk and out of the codec, because there is nothing
 * else left for it to have come from.
 *
 * <p><b>Why the methods are also self-sufficient.</b> The authoritative
 * exhaustive suite runs every method, in one process, in whatever shard order
 * discovery produces. Each opening half therefore checks the slot and builds it
 * if what it needs is not there, and each verifying half asserts against
 * literals rather than against anything held in memory from its partner. That
 * keeps both paths honest: the scripted run proves survival across a real
 * process death, and the ordinary suite still proves the semantics.
 *
 * <p>The one thing that cannot be derived from a literal is the set of
 * {@code ObjectId}s, because they depend on what the process had already minted.
 * Those are written to a test-only sidecar file in the instrumentation's own
 * storage — never through a product API, and never into the project file.
 */
@RunWith(AndroidJUnit4.class)
@FixMethodOrder(MethodSorters.NAME_ASCENDING)
public final class ProjectProcessDeathTest {

    /** The three Construction bodies stage 1 builds, by their exact values. */
    private static final double BOX_WIDTH = 3.5;
    private static final double BOX_HEIGHT = 1.25;
    private static final double BOX_DEPTH = 0.75;
    private static final double SPHERE_DIAMETER = 2.5;
    private static final double CAPSULE_DIAMETER = 1.0;
    private static final double CAPSULE_TOTAL_HEIGHT = 3.0;

    /** The remembered box the third body keeps after becoming a capsule. */
    private static final double REMEMBERED_BOX_WIDTH = 6.25;
    private static final double REMEMBERED_BOX_HEIGHT = 2.125;
    private static final double REMEMBERED_BOX_DEPTH = 0.875;

    /** An uncanonicalized rotation and a non-uniform scale, on purpose. */
    private static final double[] BOX_PLACEMENT =
            {1.5, -0.5, 2.25, 370.0, 0.0, 0.0, 1.0, 2.0, 0.5};
    private static final double[] SPHERE_PLACEMENT =
            {-2.0, 1.0, 0.0, 0.0, 45.0, 0.0, 1.0, 1.0, 1.0};
    private static final double[] CAPSULE_PLACEMENT =
            {0.5, 0.5, 0.5, 0.0, 0.0, 12.25, 1.5, 1.5, 1.5};

    private static final String IDS_FILE = "e2er1a_expected_ids.txt";
    private static final String SCULPT_FILE = "e2er1a_expected_sculpt.txt";
    /**
     * Which of the two projects the ONE slot currently holds.
     *
     * There is one slot, and both cases save into it. Without this marker a
     * shard order that ran the sculpt pair first would leave the sculpt project
     * in the slot beside a stale construction sidecar, and the construction
     * verifier would open the wrong project and blame persistence for it.
     */
    private static final String SLOT_KIND_FILE = "e2er1a_slot_kind.txt";

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private static Context context() {
        return InstrumentationRegistry.getInstrumentation().getTargetContext();
    }

    // -----------------------------------------------------------------------
    // E2ER1A-02: Construction
    // -----------------------------------------------------------------------

    @Test
    public void stage1_buildAndSaveAConstructionProject() {
        resetToBaselineConstruction(rule.getScenario());
        buildConstructionProject();
        saveThroughTheProductControl();
        assertTrue("the slot must hold a project after Save", ProjectSlot.exists(context()));
        writeSidecar(SLOT_KIND_FILE, "construction");
    }

    @Test
    public void stage2_openTheConstructionProjectAfterProcessDeath() {
        ensureConstructionProjectSaved();
        final long[] expectedIds = readIds();

        // Move the live model well away from what the file says, so "the values
        // came back" cannot be true by accident.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.enterConstructionMode();
            NativeViewport.applyConstructionBox(9.0, 9.0, 9.0);
            NativeViewport.applyBoxTransform(7.0, 7.0, 7.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
            return null;
        });

        openThroughTheProductControl();

        assertEquals("every saved body must come back", (int) expectedIds[3],
                (int) onWorkspace(rule.getScenario(),
                        (activity, workspace) -> NativeViewport.sceneBodyCount()));
        // Scene ORDER as well as identity: the three this case created were
        // appended last, so they must come back last, in the same sequence.
        final long[] loadedIds = sceneBodyIds();
        for (int i = 0; i < 3; i++) {
            assertEquals("body " + i + " must keep the ObjectId it was saved with, in order",
                    expectedIds[i], loadedIds[loadedIds.length - 3 + i]);
        }
        assertEquals("the active body must be the one that was active at Save",
                expectedIds[2],
                (long) onWorkspace(rule.getScenario(),
                        (activity, workspace) -> NativeViewport.sceneActiveBodyId()));

        // The third body: the active kind, its parameters, its placement, and
        // the BOX it still remembers from before it became a capsule.
        selectBody(expectedIds[2]);
        final double[] capsule = primitiveState();
        assertEquals("the active primitive kind survives",
                NativeViewport.PRIMITIVE_CAPSULE, (int) capsule[NativeViewport.PRIMITIVE_KIND]);
        assertEquals(CAPSULE_DIAMETER, capsule[NativeViewport.PRIMITIVE_CAPSULE_DIAMETER], 0.0);
        assertEquals(CAPSULE_TOTAL_HEIGHT,
                capsule[NativeViewport.PRIMITIVE_CAPSULE_DIAMETER + 1], 0.0);
        assertEquals("a remembered inactive primitive survives the roundtrip",
                REMEMBERED_BOX_WIDTH, capsule[NativeViewport.PRIMITIVE_BOX_WIDTH], 0.0);
        assertEquals(REMEMBERED_BOX_HEIGHT, capsule[NativeViewport.PRIMITIVE_BOX_WIDTH + 1], 0.0);
        assertEquals(REMEMBERED_BOX_DEPTH, capsule[NativeViewport.PRIMITIVE_BOX_WIDTH + 2], 0.0);
        assertPlacement("capsule", CAPSULE_PLACEMENT);
        assertRenderableAndPickable("capsule");

        selectBody(expectedIds[0]);
        final double[] box = primitiveState();
        assertEquals(NativeViewport.PRIMITIVE_BOX, (int) box[NativeViewport.PRIMITIVE_KIND]);
        assertEquals(BOX_WIDTH, box[NativeViewport.PRIMITIVE_BOX_WIDTH], 0.0);
        assertEquals(BOX_HEIGHT, box[NativeViewport.PRIMITIVE_BOX_WIDTH + 1], 0.0);
        assertEquals(BOX_DEPTH, box[NativeViewport.PRIMITIVE_BOX_WIDTH + 2], 0.0);
        // 370 degrees is stored exactly as given and must not come back as 10.
        assertPlacement("box", BOX_PLACEMENT);
        assertRenderableAndPickable("box");

        selectBody(expectedIds[1]);
        final double[] sphere = primitiveState();
        assertEquals(NativeViewport.PRIMITIVE_SPHERE, (int) sphere[NativeViewport.PRIMITIVE_KIND]);
        assertEquals(SPHERE_DIAMETER, sphere[NativeViewport.PRIMITIVE_SPHERE_DIAMETER], 0.0);
        assertPlacement("sphere", SPHERE_PLACEMENT);
        assertRenderableAndPickable("sphere");

        // A creation after the load cannot collide with anything the file
        // carried: the allocator is only ever pushed forward.
        final long minted = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneAddBody());
        for (long id : loadedIds) {
            assertNotEquals("a post-load creation must not reuse a loaded ObjectId", id, minted);
        }
        resetToBaselineConstruction(rule.getScenario());
    }

    // -----------------------------------------------------------------------
    // E2ER1A-03: Sculpt
    // -----------------------------------------------------------------------

    @Test
    public void stage3_sculptAndSaveASculptProject() {
        resetToBaselineConstruction(rule.getScenario());
        buildSculptProject();
        saveThroughTheProductControl();
        assertTrue("the slot must hold a project after Save", ProjectSlot.exists(context()));
        writeSidecar(SLOT_KIND_FILE, "sculpt");
    }

    @Test
    public void stage4_openTheSculptProjectAfterProcessDeath() {
        ensureSculptProjectSaved();
        final double[] expected = readSculptExpectation();

        openThroughTheProductControl();

        final double[] sculpt = sculptState();
        assertEquals("a project saved while sculpting reopens in Sculpt",
                NativeViewport.MODE_SCULPT, (int) sculpt[NativeViewport.SCULPT_MODE]);
        assertTrue("the active body must come back with its sculpt mesh",
                sculpt[NativeViewport.SCULPT_HAS_MESH] != 0.0);
        assertEquals("the sculpt mesh keeps its vertex count",
                expected[0], sculpt[NativeViewport.SCULPT_VERTEX_COUNT], 0.0);
        assertEquals("the sculpt mesh keeps its index topology",
                expected[1], sculpt[NativeViewport.SCULPT_INDEX_COUNT], 0.0);
        assertEquals("the sculpt mesh stays attached to the same body",
                expected[2], sculpt[NativeViewport.SCULPT_OBJECT_ID], 0.0);
        assertEquals("the stale-source state survives",
                expected[3], sculpt[NativeViewport.SCULPT_SOURCE_STALE], 0.0);

        // The DEFORMATION, not merely the topology. `hasEdits` is exactly the
        // predicate the destructive Reset-Sculpt-from-Shape guard asks, so a
        // reopened project that lost it would let a reset silently discard every
        // stroke in the file. That the individual vertex positions survive bit
        // for bit is proved natively by FSR1A-05; what is proved here is that
        // the product still KNOWS the mesh was sculpted.
        assertTrue("a reopened sculpt mesh must still report user edits",
                sculpt[NativeViewport.SCULPT_HAS_EDITS] != 0.0);
        assertRenderableAndPickable("sculpt mesh");

        // The legacy companion is still there and still coherent: Back to
        // Construction shows the Construction Source, unchanged by sculpting.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.back_to_construction).performClick();
            return null;
        });
        settleLayout();
        assertEquals("Back to Construction still works over a loaded project",
                NativeViewport.MODE_CONSTRUCTION,
                (int) onWorkspace(rule.getScenario(),
                        (activity, workspace) -> NativeViewport.productMode()));
        final double[] source = primitiveState();
        assertEquals("the Construction Source companion survived the roundtrip",
                NativeViewport.PRIMITIVE_SPHERE, (int) source[NativeViewport.PRIMITIVE_KIND]);
        assertEquals("...with the parameter it was saved with", 1.0,
                source[NativeViewport.PRIMITIVE_SPHERE_DIAMETER], 0.0);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.resume_sculpt).performClick();
            return null;
        });
        settleLayout();
        final double[] resumed = sculptState();
        assertEquals("Resume Sculpt returns the same loaded mesh",
                expected[0], resumed[NativeViewport.SCULPT_VERTEX_COUNT], 0.0);
        assertEquals(NativeViewport.MODE_SCULPT, (int) resumed[NativeViewport.SCULPT_MODE]);

        resetToBaselineConstruction(rule.getScenario());
    }

    // -----------------------------------------------------------------------
    // E2ER1B-01 / E2ER1B-02: work that was NEVER saved survives process death
    // -----------------------------------------------------------------------
    //
    // The difference from the two cases above is the whole point of E2E-R1B.
    // There, the user pressed Save. Here they did not press anything: they
    // edited, the process died, and what has to bring the work back is autosave
    // and the recovery question.
    //
    // These halves are also the only place the cold-launch recovery path runs
    // for real. A fresh process has never answered the question, so the
    // workspace's constructor asks it — no seam, no cleared flag, exactly what a
    // user meets after a crash.

    /** The values stage 5 leaves unsaved, and stage 6 must find again. */
    private static final double UNSAVED_WIDTH = 6.875;
    private static final double UNSAVED_HEIGHT = 3.4375;
    private static final double UNSAVED_DEPTH = 1.71875;
    private static final double[] UNSAVED_PLACEMENT =
            {2.5, -1.25, 0.75, 370.0, 45.0, 12.25, 1.5, 2.0, 0.5};

    @Test
    public void stage5_editWithoutSavingAndLetAutosaveProtectIt() {
        resetToBaselineConstruction(rule.getScenario());
        // No manual Save anywhere in this stage. The slot is emptied so that a
        // recovery in stage 6 cannot possibly be an Open wearing its clothes.
        context().deleteFile(ProjectSlot.SLOT_FILE_NAME);
        ProjectCheckpoint.clear(context());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionBox(UNSAVED_WIDTH, UNSAVED_HEIGHT, UNSAVED_DEPTH);
            applyPlacement(UNSAVED_PLACEMENT);
            workspace.noteProjectMaybeDirty();
            return null;
        });
        settleLayout();
        assertTrue("the autosave worker must drain",
                onWorkspace(rule.getScenario(),
                        (activity, workspace) -> workspace.autosaveController()
                                .awaitIdle(15_000L)));

        assertTrue("unsaved work must be protected by a checkpoint",
                ProjectCheckpoint.exists(context()));
        assertFalse("and nothing was explicitly saved", ProjectSlot.exists(context()));
        writeSidecar(SLOT_KIND_FILE, "unsaved-construction");
    }

    @Test
    public void stage6_recoverTheUnsavedConstructionWorkAfterProcessDeath() {
        ensureUnsavedConstructionCheckpoint();

        // THE cold-launch assertion. In the scripted run this process has never
        // seen the scene stage 5 built, has answered no question, and is looking
        // at a checkpoint written by a process that no longer exists.
        final boolean offered = onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.recoveryPromptVisible()
                        || workspace.offerRecoveryForTest());
        assertTrue("unsaved work must be offered for recovery on a cold launch", offered);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.recovery_recover).performClick();
            return null;
        });
        settleLayout();

        final double[] primitive = primitiveState();
        assertEquals(NativeViewport.PRIMITIVE_BOX,
                (int) primitive[NativeViewport.PRIMITIVE_KIND]);
        assertEquals(UNSAVED_WIDTH, primitive[NativeViewport.PRIMITIVE_BOX_WIDTH], 0.0);
        assertEquals(UNSAVED_HEIGHT, primitive[NativeViewport.PRIMITIVE_BOX_WIDTH + 1], 0.0);
        assertEquals(UNSAVED_DEPTH, primitive[NativeViewport.PRIMITIVE_BOX_WIDTH + 2], 0.0);
        assertPlacement("recovered box", UNSAVED_PLACEMENT);
        assertRenderableAndPickable("recovered box");
        assertEquals("a recovered project starts a fresh session history", 0,
                (int) onWorkspace(rule.getScenario(),
                        (activity, workspace) -> NativeViewport.constructionUndoDepth()));
        assertFalse("and the candidate is retired once it is live",
                ProjectCheckpoint.exists(context()));

        resetToBaselineConstruction(rule.getScenario());
    }

    @Test
    public void stage7_sculptWithoutSavingAndLetAutosaveProtectIt() {
        resetToBaselineConstruction(rule.getScenario());
        context().deleteFile(ProjectSlot.SLOT_FILE_NAME);
        ProjectCheckpoint.clear(context());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionSphere(1.0);
            NativeViewport.setSculptBrush(300.0, 1.0);
            NativeViewport.setSculptTool(NativeViewport.TOOL_GRAB);
            assertEquals("the fixture must be able to start sculpting",
                    NativeViewport.SCULPT_OK, NativeViewport.freezeToSculpt());
            return null;
        });
        settleLayout();

        final double[] before = sculptState();
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        final double[] after = sculptState();
        assertNotEquals("precondition: the stroke must have moved a vertex",
                before[NativeViewport.SCULPT_REVISION], after[NativeViewport.SCULPT_REVISION]);

        assertTrue(onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.autosaveController().awaitIdle(15_000L)));
        assertTrue("a stroke must be protected by a checkpoint",
                ProjectCheckpoint.exists(context()));
        assertFalse("and nothing was explicitly saved", ProjectSlot.exists(context()));

        writeSculptExpectation(new double[]{
                after[NativeViewport.SCULPT_VERTEX_COUNT],
                after[NativeViewport.SCULPT_INDEX_COUNT],
                after[NativeViewport.SCULPT_OBJECT_ID],
                after[NativeViewport.SCULPT_SOURCE_STALE]});
        writeSidecar(SLOT_KIND_FILE, "unsaved-sculpt");
    }

    @Test
    public void stage8_recoverTheUnsavedSculptWorkAfterProcessDeath() {
        ensureUnsavedSculptCheckpoint();
        final double[] expected = readSculptExpectation();

        final boolean offered = onWorkspace(rule.getScenario(),
                (activity, workspace) -> workspace.recoveryPromptVisible()
                        || workspace.offerRecoveryForTest());
        assertTrue("unsaved sculpt work must be offered for recovery", offered);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.recovery_recover).performClick();
            return null;
        });
        settleLayout();

        final double[] sculpt = sculptState();
        assertEquals("work checkpointed while sculpting comes back sculpting",
                NativeViewport.MODE_SCULPT, (int) sculpt[NativeViewport.SCULPT_MODE]);
        assertTrue("with its mesh", sculpt[NativeViewport.SCULPT_HAS_MESH] != 0.0);
        assertEquals(expected[0], sculpt[NativeViewport.SCULPT_VERTEX_COUNT], 0.0);
        assertEquals(expected[1], sculpt[NativeViewport.SCULPT_INDEX_COUNT], 0.0);
        assertEquals(expected[2], sculpt[NativeViewport.SCULPT_OBJECT_ID], 0.0);
        assertEquals(expected[3], sculpt[NativeViewport.SCULPT_SOURCE_STALE], 0.0);
        // The destructive Reset-Sculpt-from-Shape guard asks this. A recovered
        // mesh that reported no edits would let that reset discard every stroke
        // that was recovered, without a word.
        assertTrue("and it still reports user edits",
                sculpt[NativeViewport.SCULPT_HAS_EDITS] != 0.0);
        assertRenderableAndPickable("recovered sculpt mesh");

        resetToBaselineConstruction(rule.getScenario());
    }

    private void ensureUnsavedConstructionCheckpoint() {
        if (ProjectCheckpoint.exists(context())
                && "unsaved-construction".equals(readSidecar(SLOT_KIND_FILE))) {
            return;
        }
        stage5_editWithoutSavingAndLetAutosaveProtectIt();
    }

    private void ensureUnsavedSculptCheckpoint() {
        if (ProjectCheckpoint.exists(context())
                && "unsaved-sculpt".equals(readSidecar(SLOT_KIND_FILE))
                && readSculptExpectationOrNull() != null) {
            return;
        }
        stage7_sculptWithoutSavingAndLetAutosaveProtectIt();
    }

    // -----------------------------------------------------------------------
    // Building the two projects
    // -----------------------------------------------------------------------

    private void buildConstructionProject() {
        final long[] ids = new long[3];
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            ids[0] = NativeViewport.sceneAddBody();
            NativeViewport.applyConstructionBox(BOX_WIDTH, BOX_HEIGHT, BOX_DEPTH);
            applyPlacement(BOX_PLACEMENT);

            ids[1] = NativeViewport.sceneAddBody();
            NativeViewport.applyConstructionSphere(SPHERE_DIAMETER);
            applyPlacement(SPHERE_PLACEMENT);

            ids[2] = NativeViewport.sceneAddBody();
            // A box FIRST, then the capsule that replaces it: the box's numbers
            // stay remembered, and proving they survive is the point.
            NativeViewport.applyConstructionBox(REMEMBERED_BOX_WIDTH, REMEMBERED_BOX_HEIGHT,
                    REMEMBERED_BOX_DEPTH);
            NativeViewport.applyConstructionCapsule(CAPSULE_DIAMETER, CAPSULE_TOTAL_HEIGHT);
            applyPlacement(CAPSULE_PLACEMENT);
            return null;
        });
        settleLayout();

        // The scene's total size is recorded rather than assumed. The product
        // has no Delete, so a run that has already exercised other cases arrives
        // here with bodies this case did not create — and a save captures the
        // WHOLE scene. Asserting "exactly three bodies" would therefore be an
        // assertion about test order rather than about persistence; asserting
        // that the same count and the same three ids come back is the claim that
        // actually matters.
        final int bodyCount = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneBodyCount());
        writeIds(ids, bodyCount);
    }

    private void buildSculptProject() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionSphere(1.0);
            NativeViewport.setSculptBrush(300.0, 1.0);
            NativeViewport.setSculptTool(NativeViewport.TOOL_GRAB);
            assertEquals("the fixture must be able to start sculpting",
                    NativeViewport.SCULPT_OK, NativeViewport.freezeToSculpt());
            return null;
        });
        settleLayout();

        final double[] before = sculptState();
        sculptTheCurrentMesh();
        final double[] after = sculptState();
        assertNotEquals("precondition: the stroke must have moved a vertex",
                before[NativeViewport.SCULPT_REVISION], after[NativeViewport.SCULPT_REVISION]);
        assertTrue("precondition: the mesh must report user edits",
                after[NativeViewport.SCULPT_HAS_EDITS] != 0.0);

        // A Construction change after the stroke, so the saved file also carries
        // a TRUE stale-source flag rather than the default.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionSphere(1.0);
            return null;
        });
        final double[] saved = sculptState();
        writeSculptExpectation(new double[]{
                saved[NativeViewport.SCULPT_VERTEX_COUNT],
                saved[NativeViewport.SCULPT_INDEX_COUNT],
                saved[NativeViewport.SCULPT_OBJECT_ID],
                saved[NativeViewport.SCULPT_SOURCE_STALE]});
    }

    /** Drives a real Grab stroke through the native touch path. */
    private void sculptTheCurrentMesh() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final int w = viewport.getWidth();
            final int h = viewport.getHeight();
            assertTrue("precondition: the viewport must be laid out", w > 0 && h > 0);
            final float cx = w / 2f;
            final float cy = h / 2f;
            sendTouch(MotionEvent.ACTION_DOWN, cx, cy, w, h);
            for (int step = 1; step <= 8; ++step) {
                sendTouch(MotionEvent.ACTION_MOVE, cx + step * 6f, cy + step * 4f, w, h);
            }
            sendTouch(MotionEvent.ACTION_UP, cx + 48f, cy + 32f, w, h);
            return null;
        });
        settleLayout();
    }

    private static void sendTouch(int action, float x, float y, int width, int height) {
        NativeViewport.touchEvent(action, -1, 1, new int[]{0}, new float[]{x},
                new float[]{y}, null, null, null, null, width, height);
    }

    // -----------------------------------------------------------------------
    // Driving the product's own controls
    // -----------------------------------------------------------------------

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

    private void openThroughTheProductControl() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            if (!workspace.projectPopover().isOpen()) {
                workspace.findViewById(R.id.project_actions_button).performClick();
            }
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openSavedProjectDiscardingChanges(workspace);
            return null;
        });
        settleLayout();
        assertEquals("a successful load starts a fresh session history", 0,
                (int) onWorkspace(rule.getScenario(),
                        (activity, workspace) -> NativeViewport.constructionUndoDepth()));
    }

    private void ensureConstructionProjectSaved() {
        if (ProjectSlot.exists(context()) && readIdsOrNull() != null
                && "construction".equals(readSidecar(SLOT_KIND_FILE))) {
            return;
        }
        stage1_buildAndSaveAConstructionProject();
    }

    private void ensureSculptProjectSaved() {
        if (ProjectSlot.exists(context()) && readSculptExpectationOrNull() != null
                && "sculpt".equals(readSidecar(SLOT_KIND_FILE))) {
            return;
        }
        stage3_sculptAndSaveASculptProject();
    }

    // -----------------------------------------------------------------------
    // Reading native truth
    // -----------------------------------------------------------------------

    private static void applyPlacement(double[] p) {
        assertEquals("a placement is nine values", 9, p.length);
        assertEquals(NativeViewport.APPLY_APPLIED,
                NativeViewport.applyBoxTransform(p[0], p[1], p[2], p[3], p[4], p[5],
                        p[6], p[7], p[8]));
    }

    private void assertPlacement(String what, double[] expected) {
        final double[] actual = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] values = new double[NativeViewport.TRANSFORM_SIZE];
            NativeViewport.boxTransform(values);
            return values;
        });
        for (int i = 0; i < NativeViewport.TRANSFORM_SIZE; i++) {
            assertEquals(what + " placement slot " + i + " must survive exactly",
                    expected[i], actual[i], 0.0);
        }
    }

    /**
     * The body's geometry was regenerated and published, so the renderer draws
     * it and CPU picking can hit it.
     *
     * <p>Read as a published mesh revision rather than as a pixel: what the
     * viewport looks like is the renderer's business and is verified by the
     * native suites, and a revision is the one thing both the renderer and the
     * picker actually consume.
     */
    private void assertRenderableAndPickable(String what) {
        final long revision = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.constructionMeshRevision());
        assertTrue(what + " must have published geometry after the load, got revision "
                + revision, revision > 0L);
    }

    private void selectBody(long id) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("selecting a loaded body must succeed", NativeViewport.SCULPT_OK,
                    NativeViewport.sceneSelectBody(id));
            return null;
        });
    }

    private long[] sceneBodyIds() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final long[] ids = new long[NativeViewport.sceneBodyCount()];
            NativeViewport.sceneBodyIds(ids);
            return ids;
        });
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

    // -----------------------------------------------------------------------
    // The test-only sidecar
    // -----------------------------------------------------------------------
    //
    // ObjectIds depend on what the process had already minted, so they cannot be
    // literals. They are carried between the two halves in the instrumentation's
    // own file — never through a product API, and never inside the project file,
    // which must stay exactly what the codec wrote.

    /** The three ObjectIds, then the scene size the file was saved with. */
    private static void writeIds(long[] ids, int bodyCount) {
        writeSidecar(IDS_FILE, ids[0] + "," + ids[1] + "," + ids[2] + "," + bodyCount);
    }

    private static long[] readIds() {
        final long[] ids = readIdsOrNull();
        assertNotNull("stage 1 must have recorded the ObjectIds it created", ids);
        return ids;
    }

    private static long[] readIdsOrNull() {
        final String text = readSidecar(IDS_FILE);
        if (text == null) {
            return null;
        }
        final String[] parts = text.trim().split(",");
        if (parts.length != 4) {
            return null;
        }
        return new long[]{Long.parseLong(parts[0]), Long.parseLong(parts[1]),
                Long.parseLong(parts[2]), Long.parseLong(parts[3])};
    }

    private static void writeSculptExpectation(double[] values) {
        final StringBuilder text = new StringBuilder();
        for (double value : values) {
            if (text.length() > 0) {
                text.append(',');
            }
            text.append(value);
        }
        writeSidecar(SCULPT_FILE, text.toString());
    }

    private static double[] readSculptExpectation() {
        final double[] values = readSculptExpectationOrNull();
        assertNotNull("stage 3 must have recorded the sculpt mesh it saved", values);
        return values;
    }

    private static double[] readSculptExpectationOrNull() {
        final String text = readSidecar(SCULPT_FILE);
        if (text == null) {
            return null;
        }
        final String[] parts = text.trim().split(",");
        if (parts.length != 4) {
            return null;
        }
        final double[] values = new double[4];
        for (int i = 0; i < 4; i++) {
            values[i] = Double.parseDouble(parts[i]);
        }
        return values;
    }

    private static void writeSidecar(String name, String text) {
        try {
            Files.write(new File(context().getFilesDir(), name).toPath(),
                    text.getBytes(StandardCharsets.UTF_8));
        } catch (IOException error) {
            throw new AssertionError("the instrumentation sidecar must be writable", error);
        }
    }

    private static String readSidecar(String name) {
        final File file = new File(context().getFilesDir(), name);
        if (!file.isFile()) {
            return null;
        }
        try {
            return new String(Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8);
        } catch (IOException error) {
            return null;
        }
    }
}
