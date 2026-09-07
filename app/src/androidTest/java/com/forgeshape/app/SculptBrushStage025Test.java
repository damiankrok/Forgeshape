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
import android.net.Uri;
import android.view.View;
import android.widget.LinearLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

/**
 * `E2E-FCM-01..11`: Flatten, Crease and the Sculpt Mask on a device
 * (`SCULPT-FCM-R1`).
 *
 * <h2>What this suite is for, and what it is not</h2>
 *
 * <p>The DOMAIN half — the flattening plane and its convergence, the two
 * components of a crease, the mask arithmetic, the {@code 1 - w} factor over
 * every geometry brush, the history's two sides, the byte budget and the
 * world/display brush metric under a non-uniform Scale — is proved by the
 * native self-tests (`FCM-01..20`), which build their own scenes and depend on
 * no live session. What is left, and what this covers, is everything that can
 * only be true on a device: the three new rail entries, the native tool
 * dispatch behind them, the real Clear Mask control, the mask living inside the
 * ONE Sculpt history through the real Undo/Redo/navigator chrome, and the
 * persistence boundary — what Back and Resume keep, and what a save and reopen
 * deliberately does not.
 *
 * <h2>How geometry and mask are observed</h2>
 *
 * <p>Geometry through the {@code .forge} document's {@code SCUL} section, which
 * is where a Frozen Sculpt Mesh's positions are project truth — the same route
 * {@code SculptUndoTest} reads it by, and the comparison is bit-exact.
 *
 * <p>The MASK cannot be observed that way, and that is the point: it reaches no
 * {@code .forge} byte at all. It is read through
 * {@code SCULPT_MASKED_VERTEX_COUNT}, which native code answers from the one
 * mask that exists.
 */
@RunWith(AndroidJUnit4.class)
public final class SculptBrushStage025Test {

    /** The same six-node import fixture the Imported Mesh suites use. */
    private static final String SIX_NODE_GLB = "glb/construction_sentinel.glb";

    /** How long the autosave worker gets to drain. Never slept for. */
    private static final long IDLE_TIMEOUT_MS = 5000L;

    /**
     * The brush setup the mask cases use, and why these three numbers.
     *
     * <p>Painting is travel-driven: one pass deposits
     * {@code strength × weight × travelPixels / radiusPixels}. The shared
     * stroke gesture is about 68 px of path, so through a 600 px brush one pass
     * paints roughly {@code 0.11 × weight}. A vertex at a fifth of the mask
     * brush's radius carries a falloff weight of {@code (1 - 0.04)² ≈ 0.92}, so
     * it saturates in about ten passes — and every vertex a 120 px geometry
     * brush can reach is inside that fifth. Twenty passes is therefore roughly
     * double what the boundary needs, which is the margin a device case wants.
     */
    private static final double GEOMETRY_RADIUS_PIXELS = 120.0;
    private static final double MASK_RADIUS_PIXELS = 600.0;
    private static final int MASK_PASSES = 20;

    /**
     * The same setup for the IMPORTED fixture, which needs a wider geometry
     * brush and therefore more passes.
     *
     * <p>The import is a six-vertex shell, not a 482-vertex sphere: a 120 px
     * brush on it captures nothing at all, and a case whose geometry stroke
     * moved nothing would report "the mask protected it" for the wrong reason.
     * So the geometry brush is the 300 px one the import helper already poses
     * for — and with the mask brush only twice its radius, the boundary vertex
     * sits at half the mask's falloff (weight ≈ 0.56) and needs about sixteen
     * passes, so thirty-two is again roughly double.
     */
    private static final double IMPORTED_GEOMETRY_RADIUS_PIXELS = 300.0;
    private static final int IMPORTED_MASK_PASSES = 32;

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private final File scratch = new File(context().getCacheDir(), "sculpt-fcm-test");

    private static Context context() {
        return InstrumentationRegistry.getInstrumentation().getTargetContext();
    }

    /** The project as this case found it. See SculptUndoTest for why. */
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
    // E2E-FCM-01 — the three new tools exist on the rail and are selectable
    // -----------------------------------------------------------------------

    /**
     * E2E-FCM-01. Sculpt offers seven brushes, each one selectable through the
     * real rail entry, and native code owns which is held.
     *
     * <p>The rail order is the product's reading order and the native index is
     * the enum's; asserting the pairing here is what keeps the two independent
     * without letting them drift apart.
     */
    @Test
    public void e2eFcm01_allSevenBrushesAreOnTheRailAndSelectable() {
        startSculptingAConstructionSphere();

        final int[] railIds = {R.id.tool_rail_grab, R.id.tool_rail_clay, R.id.tool_rail_smooth,
                R.id.tool_rail_flatten, R.id.tool_rail_inflate, R.id.tool_rail_crease,
                R.id.tool_rail_mask};
        final int[] tools = {NativeViewport.TOOL_GRAB, NativeViewport.TOOL_CLAY,
                NativeViewport.TOOL_SMOOTH, NativeViewport.TOOL_FLATTEN,
                NativeViewport.TOOL_INFLATE, NativeViewport.TOOL_CREASE,
                NativeViewport.TOOL_MASK};

        for (int i = 0; i < railIds.length; i++) {
            final int index = i;
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                final View entry = workspace.findViewById(railIds[index]);
                assertTrue("rail entry " + index + " must be drawn in Sculpt",
                        entry != null && entry.getVisibility() == View.VISIBLE);
                entry.performClick();
                return null;
            });
            settleLayout();
            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                assertEquals("E2E-FCM-01: native code owns which tool is held",
                        tools[index], NativeViewport.sculptTool());
                for (int other = 0; other < railIds.length; other++) {
                    assertEquals("exactly one entry may be drawn active",
                            other == index,
                            workspace.findViewById(railIds[other]).isActivated());
                }
                return null;
            });
        }
    }

    // -----------------------------------------------------------------------
    // E2E-FCM-02 / E2E-FCM-03 — a real Flatten and a real Crease stroke
    // -----------------------------------------------------------------------

    /**
     * E2E-FCM-02. A real Flatten stroke through the touch path moves sculpt
     * truth, and the real Undo and Redo controls take it back and put it
     * exactly where it was.
     */
    @Test
    public void e2eFcm02_aRealFlattenStrokeUndoesAndRedoesExactly() {
        startSculptingAConstructionSphere();
        selectTool(R.id.tool_rail_flatten, NativeViewport.TOOL_FLATTEN);
        assertBrushStrokeUndoesAndRedoesExactly("Flatten");
    }

    /**
     * E2E-FCM-03. The same, for Crease.
     */
    @Test
    public void e2eFcm03_aRealCreaseStrokeUndoesAndRedoesExactly() {
        startSculptingAConstructionSphere();
        selectTool(R.id.tool_rail_crease, NativeViewport.TOOL_CREASE);
        assertBrushStrokeUndoesAndRedoesExactly("Crease");
    }

    // -----------------------------------------------------------------------
    // E2E-FCM-04 / E2E-FCM-05 — the mask protects, and Clear Mask releases
    // -----------------------------------------------------------------------

    /**
     * E2E-FCM-04, E2E-FCM-05. A real Mask stroke protects the area it painted:
     * the SAME gesture with a geometry brush then changes nothing at all. The
     * real Clear Mask control releases it, that same gesture then works again,
     * and Clear Mask is itself exactly one Undo.
     *
     * <p><b>Why the brush is set up the way it is.</b> Painting is
     * travel-driven like every other path-driven brush: one pass deposits
     * {@code strength × weight × travelPixels / radiusPixels}, so the shared
     * stroke gesture — about 68 px of path — paints roughly a tenth of a full
     * mask through a 600 px brush. That is the documented rule working, not a
     * weak mask, and a user painting an area drags much further than one
     * gesture. So this case does what a user does: it paints the area
     * repeatedly, with a mask brush FIVE TIMES the radius of the geometry
     * brush, so every vertex the geometry brush can reach sits at a quarter of
     * the mask's falloff and saturates in far fewer passes than are taken.
     *
     * <p>The precondition is proved rather than assumed: the geometry stroke is
     * run and undone FIRST at exactly the radius it will be run at again, so
     * "it moved nothing" can never pass because the brush was too small to
     * capture a vertex.
     */
    @Test
    public void e2eFcm04and05_aMaskProtectsAndClearMaskReleasesAndIsUndoable() {
        startSculptingAConstructionSphere();

        // Clear Mask is ABSENT before there is a mask: a control that cannot
        // succeed is not drawn.
        openInspector();
        assertEquals("E2E-FCM-05: Clear Mask is absent with no mask",
                View.GONE, clearMaskVisibility());

        // The precondition, proved: this brush at this radius really does move
        // this mesh with this gesture.
        setBrush(GEOMETRY_RADIUS_PIXELS);
        selectTool(R.id.tool_rail_clay, NativeViewport.TOOL_CLAY);
        final byte[] seed = sculptSection();
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        assertNotEquals("precondition: the unmasked geometry stroke deforms",
                describeBytes(seed), describeBytes(sculptSection()));
        clickUndo();
        assertEquals("precondition: and Undo puts the mesh back exactly",
                describeBytes(seed), describeBytes(sculptSection()));
        clearHistoryByReFreezing();

        // Paint the area, the way a user paints one.
        setBrush(MASK_RADIUS_PIXELS);
        selectTool(R.id.tool_rail_mask, NativeViewport.TOOL_MASK);
        for (int pass = 0; pass < MASK_PASSES; pass++) {
            WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        }
        final int masked = maskedVertexCount();
        assertTrue("E2E-FCM-04: real Mask strokes painted a mask", masked > 0);
        final int maskEntries = sculptUndoDepth();
        assertTrue("E2E-FCM-04: each pass was its own history entry", maskEntries > 0);
        assertEquals("E2E-FCM-04: and painting a mask moves not one vertex",
                describeBytes(seed), describeBytes(sculptSection()));

        // The same gesture with a geometry brush now changes nothing, because
        // the vertices it would have moved are held.
        setBrush(GEOMETRY_RADIUS_PIXELS);
        selectTool(R.id.tool_rail_clay, NativeViewport.TOOL_CLAY);
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        assertEquals("E2E-FCM-04: a geometry brush over a full mask moves nothing",
                describeBytes(seed), describeBytes(sculptSection()));
        assertEquals("E2E-FCM-04: and records no entry, because it did nothing",
                maskEntries, sculptUndoDepth());

        // Clear Mask, through the real control.
        openInspector();
        assertEquals("E2E-FCM-05: Clear Mask appears once a mask exists",
                View.VISIBLE, clearMaskVisibility());
        clickClearMask();
        assertEquals("E2E-FCM-05: the mask is gone", 0, maskedVertexCount());
        assertEquals("E2E-FCM-05: as exactly one more history entry",
                maskEntries + 1, sculptUndoDepth());
        assertEquals("E2E-FCM-05: and the control is withdrawn again",
                View.GONE, clearMaskVisibility());
        assertEquals("E2E-FCM-05: clearing a mask moves no geometry",
                describeBytes(seed), describeBytes(sculptSection()));

        // The same brush stroke now works.
        selectTool(R.id.tool_rail_clay, NativeViewport.TOOL_CLAY);
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        assertNotEquals("E2E-FCM-05: with the mask cleared the brush deforms again",
                describeBytes(seed), describeBytes(sculptSection()));
        final byte[] afterRelease = sculptSection();
        assertEquals("and that stroke is the next entry",
                maskEntries + 2, sculptUndoDepth());

        // Undo walks back through the geometry stroke, then the CLEAR, then one
        // paint pass — and the mask comes back and goes again exactly.
        clickUndo();
        assertEquals("Undo took back the geometry stroke",
                describeBytes(seed), describeBytes(sculptSection()));
        assertEquals("with the mask still empty", 0, maskedVertexCount());
        clickUndo();
        assertEquals("E2E-FCM-05: Undo over Clear Mask puts the whole mask back",
                masked, maskedVertexCount());
        assertEquals("none of which moved one vertex",
                describeBytes(seed), describeBytes(sculptSection()));

        clickRedo();
        assertEquals("E2E-FCM-05: Redo clears it again", 0, maskedVertexCount());
        clickRedo();
        assertEquals("E2E-FCM-05: and the geometry stroke comes back exactly",
                describeBytes(afterRelease), describeBytes(sculptSection()));
    }

    // -----------------------------------------------------------------------
    // E2E-FCM-06 / E2E-FCM-07 — the navigator, and the abandoned future
    // -----------------------------------------------------------------------

    /**
     * E2E-FCM-06, E2E-FCM-07. The History navigator lists a MIXED branch of
     * geometry and mask acts as one line of states, a tap jumps across a mask
     * entry, and a new stroke after a jump drops the abandoned future.
     *
     * <p>There is no second stack for it to list: the row count is the one
     * history's {@code undo + redo + 1}, whatever kind of act each entry
     * describes.
     */
    @Test
    public void e2eFcm06and07_theNavigatorListsAndJumpsThroughMaskEntries() {
        startSculptingAConstructionSphere();

        selectTool(R.id.tool_rail_clay, NativeViewport.TOOL_CLAY);
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        final byte[] afterClay = sculptSection();

        selectTool(R.id.tool_rail_mask, NativeViewport.TOOL_MASK);
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        final int masked = maskedVertexCount();
        assertTrue("precondition: the mask stroke painted", masked > 0);

        assertEquals("E2E-FCM-06: two acts of two kinds are two entries in ONE history",
                2, sculptUndoDepth());
        assertEquals("and there is no second stack behind them", 0, sculptRedoDepth());

        openNavigator();
        assertEquals("E2E-FCM-06: two entries are three rows", 3, rowCount());
        assertEquals("with the cursor on the newest", 2, currentRowIndex());

        // Jump back across the mask entry to the state after the Clay stroke.
        tapRow(1);
        assertEquals("E2E-FCM-06: the jump landed on row 1", 1, currentRowIndex());
        assertEquals("E2E-FCM-06: which is the state after the geometry stroke",
                describeBytes(afterClay), describeBytes(sculptSection()));
        assertEquals("E2E-FCM-06: with the mask taken back too", 0, maskedVertexCount());
        assertEquals("E2E-FCM-06: and the rows are unchanged — a jump lists, it does not record",
                3, rowCount());

        // Forward again, exactly.
        tapRow(2);
        assertEquals("E2E-FCM-06: the forward jump restored the mask", masked,
                maskedVertexCount());
        assertEquals("and the geometry it stood on", describeBytes(afterClay),
                describeBytes(sculptSection()));

        // E2E-FCM-07: jump back, then take a new stroke. The future is gone.
        tapRow(1);
        assertEquals("precondition: a redo exists", 1, sculptRedoDepth());
        closeNavigator();
        selectTool(R.id.tool_rail_crease, NativeViewport.TOOL_CREASE);
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        assertEquals("E2E-FCM-07: a new stroke after a jump drops the abandoned future",
                0, sculptRedoDepth());
        openNavigator();
        assertEquals("E2E-FCM-07: so the branch is three rows again", 3, rowCount());
        assertEquals("with the cursor on the newest", 2, currentRowIndex());
        closeNavigator();
    }

    // -----------------------------------------------------------------------
    // E2E-FCM-08 — an Imported Mesh gets the same brushes and the same mask
    // -----------------------------------------------------------------------

    /**
     * E2E-FCM-08. An imported body sculpts with the three new brushes and masks
     * exactly as a Construction body does, through the same controls — and the
     * imported source never moves.
     */
    @Test
    public void e2eFcm08_anImportedMeshGetsTheSameBrushesAndTheSameMask() {
        importOneBody();
        startSculpting();

        final byte[] importedBefore = importedSection();
        assertTrue("precondition: the body has an imported branch", importedBefore.length > 0);

        setBrush(IMPORTED_GEOMETRY_RADIUS_PIXELS);
        selectTool(R.id.tool_rail_flatten, NativeViewport.TOOL_FLATTEN);
        final byte[] seed = sculptSection();
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        assertNotEquals("E2E-FCM-08: Flatten deforms an imported body's sculpt mesh",
                describeBytes(seed), describeBytes(sculptSection()));
        final byte[] afterFlatten = sculptSection();

        // The same painted-area setup E2E-FCM-04 uses, and for the same reason:
        // a mask brush wider than the geometry brush, painted over enough
        // passes to saturate everything the geometry brush can reach. The ratio
        // is two rather than five here, so the pass count is doubled — see the
        // IMPORTED_* constants.
        setBrush(MASK_RADIUS_PIXELS);
        selectTool(R.id.tool_rail_mask, NativeViewport.TOOL_MASK);
        for (int pass = 0; pass < IMPORTED_MASK_PASSES; pass++) {
            WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        }
        assertTrue("E2E-FCM-08: and it masks", maskedVertexCount() > 0);

        setBrush(IMPORTED_GEOMETRY_RADIUS_PIXELS);
        selectTool(R.id.tool_rail_crease, NativeViewport.TOOL_CREASE);
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        assertEquals("E2E-FCM-08: with the mask holding Crease off exactly as it holds Clay",
                describeBytes(afterFlatten), describeBytes(sculptSection()));

        assertEquals("E2E-FCM-08: and no sculpt act ever touched the imported source",
                describeBytes(importedBefore), describeBytes(importedSection()));
    }

    // -----------------------------------------------------------------------
    // E2E-FCM-09 / E2E-FCM-10 — the persistence boundary
    // -----------------------------------------------------------------------

    /**
     * E2E-FCM-09, E2E-FCM-10. Back to Construction and Resume Sculpt keep the
     * runtime mask; a save and reopen keeps the MESH and starts unmasked.
     *
     * <p>The document is compared byte for byte against one encoded before the
     * mask existed: an encoder that leaked one mask weight could not produce
     * identical bytes. The autosave checkpoint is checked too, because a mask
     * that moved the project fingerprint would earn a checkpoint of its own.
     */
    @Test
    public void e2eFcm09and10_backAndResumeKeepTheMaskAndAReopenStartsEmpty() {
        startSculptingAConstructionSphere();

        selectTool(R.id.tool_rail_clay, NativeViewport.TOOL_CLAY);
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        awaitAutosaveIdle();
        final byte[] documentBeforeMask = encodeProject();
        final byte[] checkpointBeforeMask = ProjectCheckpoint.read(context());
        assertTrue("precondition: a sculpted project is checkpointed",
                checkpointBeforeMask != null && checkpointBeforeMask.length > 0);

        selectTool(R.id.tool_rail_mask, NativeViewport.TOOL_MASK);
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        final int masked = maskedVertexCount();
        assertTrue("precondition: a mask exists", masked > 0);

        // The mask is in NO byte of the document.
        assertEquals("E2E-FCM-10: a mask changes not one byte of the project",
                describeBytes(documentBeforeMask), describeBytes(encodeProject()));
        awaitAutosaveIdle();
        assertEquals("E2E-FCM-10: and earns no checkpoint of its own",
                describeBytes(checkpointBeforeMask),
                describeBytes(ProjectCheckpoint.read(context())));

        // E2E-FCM-09: leaving Sculpt is navigation and takes nothing away.
        backToSource();
        resumeSculpt();
        assertEquals("E2E-FCM-09: Resume comes back to the same runtime mask",
                masked, maskedVertexCount());
        openInspector();
        assertEquals("E2E-FCM-09: with the Clear Mask control still offered",
                View.VISIBLE, clearMaskVisibility());

        // E2E-FCM-10: reopen through the ordinary atomic load.
        final byte[] saved = encodeProject();
        final File copy = new File(scratch, "sculpt-fcm.forge");
        writeFile(copy, saved);
        resetToBaselineConstruction(rule.getScenario());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.PROJECT_OK, NativeViewport.loadProject(readFile(copy)));
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        enterSculptIfNeeded();

        assertEquals("E2E-FCM-10: the reopened geometry is what was on screen",
                describeBytes(sectionPayload(saved, "SCUL")), describeBytes(sculptSection()));
        assertEquals("E2E-FCM-10: but the reopened mask is empty", 0, maskedVertexCount());
        assertEquals("E2E-FCM-10: and the sculpt history starts empty too",
                0, sculptUndoDepth());
        openInspector();
        assertEquals("E2E-FCM-10: so Clear Mask is absent again",
                View.GONE, clearMaskVisibility());
    }

    // -----------------------------------------------------------------------
    // E2E-FCM-11 — the new brushes under a non-uniform Scale
    // -----------------------------------------------------------------------

    /**
     * E2E-FCM-11. A smoke test that the three new brushes work on a body
     * carrying a non-uniform Scale, and that sculpting one bakes nothing into
     * the placement.
     *
     * <p>That the FOOTPRINT is round in world space under such a Scale is the
     * native suite's subject (`FCM-20`, which runs the Stage 020R3 assertions
     * over all seven tools). What this adds is that the whole device path —
     * touch, stroke, publish, document — survives it.
     */
    @Test
    public void e2eFcm11_theNewBrushesWorkUnderANonUniformScale() {
        startSculptingAConstructionSphere();
        backToSource();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertAccepted("the stretched pose", NativeViewport.applyBoxTransform(
                    0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 2.5, 1.0, 1.0));
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        startSculpting();
        final double[] scaleBefore = placement();

        for (final int[] tool : new int[][]{
                {R.id.tool_rail_flatten, NativeViewport.TOOL_FLATTEN},
                {R.id.tool_rail_crease, NativeViewport.TOOL_CREASE}}) {
            selectTool(tool[0], tool[1]);
            final byte[] before = sculptSection();
            WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
            assertNotEquals("E2E-FCM-11: the brush deforms a stretched body",
                    describeBytes(before), describeBytes(sculptSection()));
        }

        selectTool(R.id.tool_rail_mask, NativeViewport.TOOL_MASK);
        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        assertTrue("E2E-FCM-11: and Mask paints one", maskedVertexCount() > 0);

        final double[] scaleAfter = placement();
        for (int i = 0; i < scaleBefore.length; i++) {
            assertEquals("E2E-FCM-11: sculpting bakes nothing into the placement",
                    scaleBefore[i], scaleAfter[i], 1e-9);
        }
        assertEquals("E2E-FCM-11: and the Scale is still the non-uniform one",
                2.5, scaleAfter[6], 1e-9);
    }

    // -----------------------------------------------------------------------
    // Driving
    // -----------------------------------------------------------------------

    /**
     * One brush's whole device contract: a real stroke moves sculpt truth, the
     * real Undo puts it back exactly, and the real Redo takes it away again.
     */
    private void assertBrushStrokeUndoesAndRedoesExactly(String brush) {
        final byte[] seed = sculptSection();
        assertTrue("precondition: the project carries a SCUL section", seed.length > 0);

        WorkspaceTestSupport.sculptTheViewport(rule.getScenario());
        final byte[] afterStroke = sculptSection();
        assertNotEquals(brush + ": a real stroke moved the sculpt mesh",
                describeBytes(seed), describeBytes(afterStroke));
        assertEquals(brush + ": one completed stroke is one entry", 1, sculptUndoDepth());

        clickUndo();
        assertEquals(brush + ": Undo restored the exact pre-stroke geometry",
                describeBytes(seed), describeBytes(sculptSection()));
        clickRedo();
        assertEquals(brush + ": Redo restored the exact post-stroke geometry",
                describeBytes(afterStroke), describeBytes(sculptSection()));
    }

    /** Selects a tool the way a user does, and checks native code agrees. */
    private void selectTool(final int railId, final int expectedTool) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(railId).performClick();
            return null;
        });
        settleLayout();
        final int held = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sculptTool());
        assertEquals("native code holds the tool the rail entry asked for", expectedTool, held);
    }

    /** Sets the brush radius, at full strength. Native clamps and is the
     *  authority; this asks. */
    private void setBrush(final double radiusPixels) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.setSculptBrush(radiusPixels, 1.0);
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    /**
     * Empties the Sculpt history without changing the geometry, by re-freezing
     * the mesh that is already the source's own.
     *
     * <p>Used only to make an entry COUNT readable after a precondition stroke
     * that was undone: a Freeze clears both stacks by the existing rule, and
     * because the precondition put the mesh back exactly first, it replaces the
     * seed with an identical copy.
     */
    private void clearHistoryByReFreezing() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.freezeToSculpt());
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        assertEquals("the re-freeze cleared the undo stack", 0, sculptUndoDepth());
    }

    private void openInspector() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openPrecision(workspace);
            workspace.sculptContext().refreshFromNative();
            return null;
        });
        settleLayout();
    }

    private int clearMaskVisibility() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.sculptContext().refreshFromNative();
            return workspace.sculptContext().findViewById(R.id.clear_mask).getVisibility();
        });
    }

    private void clickClearMask() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.sculptContext().findViewById(R.id.clear_mask).performClick();
            return null;
        });
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

    private int currentRowIndex() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final LinearLayout rows = workspace.historyNavigator().rowsContainer();
            int found = -1;
            for (int i = 0; i < rows.getChildCount(); i++) {
                if (rows.getChildAt(i).isActivated()) {
                    assertEquals("exactly one row may be marked current", -1, found);
                    found = i;
                }
            }
            return found;
        });
    }

    /** Puts the active Construction body in Sculpt, on a sphere the shared
     *  stroke helper's screen centre lands on. */
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

    private static void assertAccepted(String what, int status) {
        assertTrue(what + " must be accepted, not rejected (status " + status + ")",
                status == NativeViewport.APPLY_APPLIED
                        || status == NativeViewport.APPLY_UNCHANGED);
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

    private void enterSculptIfNeeded() {
        if (productMode() == NativeViewport.MODE_SCULPT) {
            return;
        }
        resumeSculpt();
        assertEquals("the reopened project resumes into Sculpt", NativeViewport.MODE_SCULPT,
                productMode());
    }

    /** Imports the six-node fixture and leaves exactly ONE imported body
     *  selected, posed at the origin. The same isolation SculptUndoTest does. */
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

    private int maskedVertexCount() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] state = new double[NativeViewport.SCULPT_STATE_SIZE];
            NativeViewport.sculptState(state);
            return (int) state[NativeViewport.SCULPT_MASKED_VERTEX_COUNT];
        });
    }

    private int sculptUndoDepth() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sculptUndoDepth());
    }

    private int sculptRedoDepth() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sculptRedoDepth());
    }

    private int productMode() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.productMode());
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

    private byte[] sculptSection() {
        return sectionPayload(encodeProject(), "SCUL");
    }

    private byte[] importedSection() {
        return sectionPayload(encodeProject(), "IMPT");
    }

    private void awaitAutosaveIdle() {
        assertTrue("the autosave worker must drain within the timeout",
                onWorkspace(rule.getScenario(), (activity, workspace) ->
                        workspace.autosaveController().awaitIdle(IDLE_TIMEOUT_MS)));
    }

    /**
     * One section's payload out of a `.forge` file. `DATA_PACKAGE_SPEC.md` owns
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
            throw new AssertionError("the scratch file must be readable", e);
        }
    }

    private static void writeFile(File file, byte[] bytes) {
        try (FileOutputStream out = new FileOutputStream(file)) {
            out.write(bytes);
        } catch (IOException e) {
            throw new AssertionError("the scratch file must be writable", e);
        }
    }

    private static byte[] drain(InputStream in) throws IOException {
        final java.io.ByteArrayOutputStream out = new java.io.ByteArrayOutputStream();
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
        assertTrue("the scratch tree must be removable", file.delete() || !file.exists());
    }
}
