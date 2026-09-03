package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.closeAddPrimitive;
import static com.forgeshape.app.WorkspaceTestSupport.closePrecision;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.openAddPrimitive;
import static com.forgeshape.app.WorkspaceTestSupport.openObjectsPanel;
import static com.forgeshape.app.WorkspaceTestSupport.openPrecision;
import static com.forgeshape.app.WorkspaceTestSupport.closeObjectsPanel;
import static com.forgeshape.app.WorkspaceTestSupport.releaseOrientation;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.setOrientation;
import static com.forgeshape.app.WorkspaceTestSupport.settle;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static com.forgeshape.app.WorkspaceTestSupport.waitForLayout;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.content.pm.ActivityInfo;
import android.graphics.Rect;
import android.util.TypedValue;
import android.view.MotionEvent;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Stage 019: Construction Undo/Redo, from the product side of JNI.
 *
 * <p>The domain invariants — commit, cancel, redo invalidation, no-op
 * suppression, bounded capacity, stable ObjectId restoration, multi-object
 * ordering — are proved against an isolated scene by the native
 * {@code FORGESHAPE_CONSTRUCTION_HISTORY_SELFTEST} suite, which is where they
 * belong: a domain rule that can only be shown through an Android view is a rule
 * nothing else can rely on. What this suite adds is everything that is
 * genuinely about the product: that the shell routes the user's acts into one
 * native history and holds none of its own, that the two controls say what
 * native code reports, that no presentation-only action writes a step, that the
 * history survives the Android lifecycle, and that Construction Undo is never
 * offered as Sculpt Undo.
 *
 * <p>Every case starts from a reset history, because the history is
 * process-scoped exactly like the scene and "exactly one step" would otherwise
 * mean "one more than whatever the last case left".
 */
@RunWith(AndroidJUnit4.class)
public final class EditorWorkspaceHistoryTest {

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    @Before
    public void startFromABaselineBoxAndAnEmptyHistory() {
        resetToBaselineConstruction(rule.getScenario());
    }

    @After
    public void releaseTheOrientation() {
        releaseOrientation(rule.getScenario());
    }

    // -----------------------------------------------------------------------
    // S019-01, S019-27 — an empty history
    // -----------------------------------------------------------------------

    /** S019-01 / S019-27. A session that has done nothing can neither undo nor
     *  redo, and both controls say so. */
    @Test
    public void s01901_anEmptyHistoryOffersNothingAndBothControlsAreDisabled() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("a reset history holds no undo steps",
                    0, NativeViewport.constructionUndoDepth());
            assertEquals("and no redo steps",
                    0, NativeViewport.constructionRedoDepth());
            assertFalse("so nothing can be undone",
                    NativeViewport.constructionUndoAvailable());
            assertFalse("and nothing can be redone",
                    NativeViewport.constructionRedoAvailable());
            assertFalse("the Undo control is drawn disabled",
                    workspace.undoAction().isEnabled());
            assertFalse("and so is Redo", workspace.redoAction().isEnabled());
            assertEquals("asking anyway changes nothing and reports it",
                    NativeViewport.HISTORY_NOTHING_TO_DO, NativeViewport.constructionUndo());
            assertEquals(NativeViewport.HISTORY_NOTHING_TO_DO, NativeViewport.constructionRedo());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // S019-02..06 — Exact Shape
    // -----------------------------------------------------------------------

    /** S019-02, S019-03, S019-04. One valid Apply is one step, and it is exactly
     *  reversible: the kind, every canonical parameter and the ObjectId. */
    @Test
    public void s01902_oneShapeApplyIsOneReversibleStep() {
        final double[] before = onWorkspace(rule.getScenario(),
                (activity, workspace) -> constructionState());
        final long bodyBefore = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneActiveBodyId());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.APPLY_APPLIED,
                    NativeViewport.applyConstructionCylinder(0.8, 2.25));
            return null;
        });

        final double[] after = onWorkspace(rule.getScenario(),
                (activity, workspace) -> constructionState());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.syncFromNative();
            assertEquals("S019-02: exactly one history step",
                    1, NativeViewport.constructionUndoDepth());
            assertTrue("and the Undo control follows native canUndo",
                    workspace.undoAction().isEnabled());
            assertFalse("with nothing yet to redo", workspace.redoAction().isEnabled());

            assertEquals("S019-03: the step is taken", NativeViewport.HISTORY_OK,
                    NativeViewport.constructionUndo());
            assertArrayEquals("S019-03: the kind and every canonical parameter come back",
                    before, constructionState(), 0.0);
            assertEquals("S019-03: on the same body", bodyBefore,
                    NativeViewport.sceneActiveBodyId());

            assertEquals("S019-04: and redo restores the exact post-state",
                    NativeViewport.HISTORY_OK, NativeViewport.constructionRedo());
            assertArrayEquals(after, constructionState(), 0.0);
            assertEquals(bodyBefore, NativeViewport.sceneActiveBodyId());
            return null;
        });
    }

    /** S019-05, S019-06. A refused Apply and an identical Apply both write
     *  nothing, and neither publishes a mesh revision. */
    @Test
    public void s01905_rejectedAndNoOpShapeAppliesWriteNoHistory() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final long revision = NativeViewport.constructionMeshRevision();

            assertEquals("precondition: a negative width is refused",
                    NativeViewport.APPLY_REJECTED_NOT_POSITIVE,
                    NativeViewport.applyConstructionBox(-1.0, 1.0, 0.5));
            assertEquals("S019-05: a rejected Apply is not a history step",
                    0, NativeViewport.constructionUndoDepth());

            assertEquals("precondition: re-applying the same box is Unchanged",
                    NativeViewport.APPLY_UNCHANGED,
                    NativeViewport.applyConstructionBox(WorkspaceTestSupport.BASELINE_WIDTH_METERS,
                            WorkspaceTestSupport.BASELINE_HEIGHT_METERS,
                            WorkspaceTestSupport.BASELINE_DEPTH_METERS));
            assertEquals("S019-06: a no-op Apply is not a history step",
                    0, NativeViewport.constructionUndoDepth());

            assertEquals("and neither cost a mesh revision",
                    revision, NativeViewport.constructionMeshRevision());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // S019-07, S019-08 — Exact Transform
    // -----------------------------------------------------------------------

    /** S019-07, S019-08. Position and rotation move together: one Apply is one
     *  step, and undoing it returns all six values at once. */
    @Test
    public void s01907_onePlacementApplyIsOneAtomicStep() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] origin = transformValues();
            final long revision = NativeViewport.constructionMeshRevision();

            assertEquals(NativeViewport.APPLY_APPLIED,
                    NativeViewport.applyBoxTransform(1.5, -0.25, 3.0, 15.0, -90.0, 370.0, 1.0, 1.0, 1.0));
            assertEquals("S019-07: six values, one step",
                    1, NativeViewport.constructionUndoDepth());
            final double[] moved = transformValues();

            assertEquals(NativeViewport.HISTORY_OK, NativeViewport.constructionUndo());
            assertArrayEquals("S019-08: all six return together — an undo cannot "
                            + "put X back without Y and Z",
                    origin, transformValues(), 0.0);

            assertEquals(NativeViewport.HISTORY_OK, NativeViewport.constructionRedo());
            assertArrayEquals("S019-08: and all six go forward together",
                    moved, transformValues(), 0.0);
            assertEquals("370 degrees survives the round trip exactly, as the "
                            + "convention requires — history canonicalizes nothing",
                    370.0, transformValues()[5], 0.0);

            assertEquals("S019-26: a placement never publishes geometry, in either "
                            + "direction", revision, NativeViewport.constructionMeshRevision());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // S019-09, S019-10 — Add Primitive
    // -----------------------------------------------------------------------

    /** S019-09, S019-10. Choosing a Sphere from the palette is ONE creation
     *  transaction: undo removes the body outright with no default-Box remnant,
     *  and redo brings the same ObjectId back in the same place. */
    @Test
    public void s01909_addPrimitiveIsOneAtomicCreationTransaction() {
        final int bodiesBefore = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneBodyCount());
        final long activeBefore = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneActiveBodyId());

        addPrimitiveFromThePalette(R.id.add_primitive_sphere);

        final long created = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneActiveBodyId());
        final double[] createdState = onWorkspace(rule.getScenario(),
                (activity, workspace) -> constructionState());
        final long[] orderAfterAdd = onWorkspace(rule.getScenario(),
                (activity, workspace) -> sceneOrder());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("S019-09: creation is exactly one step, not two",
                    1, NativeViewport.constructionUndoDepth());
            assertNotEquals("precondition: a new body was created",
                    activeBefore, created);
            assertEquals("precondition: it is the sphere that was chosen",
                    NativeViewport.PRIMITIVE_SPHERE,
                    (int) createdState[0]);
            return null;
        });

        undoFromTheControl();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("S019-09: the body is gone as one operation",
                    bodiesBefore, NativeViewport.sceneBodyCount());
            for (long id : sceneOrder()) {
                assertNotEquals("S019-09: and nothing was left behind wearing its id — "
                                + "no temporary default Box survives the undo",
                        created, id);
            }
            assertEquals("the selection falls back to a surviving body",
                    activeBefore, NativeViewport.sceneActiveBodyId());
            return null;
        });

        redoFromTheControl();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("S019-10: the same logical body returns",
                    bodiesBefore + 1, NativeViewport.sceneBodyCount());
            assertArrayEquals("S019-10: in the same scene position, with the same ids",
                    orderAfterAdd, sceneOrder());
            assertEquals("S019-10: it is active again", created,
                    NativeViewport.sceneActiveBodyId());
            assertArrayEquals("S019-10: same primitive, same canonical parameters, "
                            + "same placement", createdState, constructionState(), 0.0);
            return null;
        });
    }

    /** S019-10b. The same path for a second primitive, so the creation
     *  transaction is not a property of Sphere. */
    @Test
    public void s01910_creationOfAPlaneUndoesAndRedoesTheSameWay() {
        final int bodiesBefore = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneBodyCount());

        addPrimitiveFromThePalette(R.id.add_primitive_plane);
        final long created = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneActiveBodyId());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(1, NativeViewport.constructionUndoDepth());
            assertEquals(NativeViewport.PRIMITIVE_PLANE,
                    (int) constructionState()[0]);
            assertEquals(NativeViewport.HISTORY_OK, NativeViewport.constructionUndo());
            assertEquals(bodiesBefore, NativeViewport.sceneBodyCount());
            assertEquals(NativeViewport.HISTORY_OK, NativeViewport.constructionRedo());
            assertEquals(bodiesBefore + 1, NativeViewport.sceneBodyCount());
            assertEquals("the same ObjectId, not a freshly minted one",
                    created, NativeViewport.sceneActiveBodyId());
            assertEquals(NativeViewport.PRIMITIVE_PLANE,
                    (int) constructionState()[0]);
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // S019-11 — multi-object isolation
    // -----------------------------------------------------------------------

    /** S019-11. Three edits across two bodies undo and redo in strict
     *  chronological order, each touching only what it recorded. */
    @Test
    public void s01911_interleavedEditsUndoAndRedoInOrderAndInIsolation() {
        final long bodyA = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneActiveBodyId());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.APPLY_APPLIED,
                    NativeViewport.applyConstructionCylinder(1.1, 2.2));
            return null;
        });
        final double[] aEdited = onWorkspace(rule.getScenario(),
                (activity, workspace) -> constructionState());

        addPrimitiveFromThePalette(R.id.add_primitive_capsule);
        final long bodyB = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneActiveBodyId());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.APPLY_APPLIED,
                    NativeViewport.applyBoxTransform(4.0, 0.0, -2.0, 0.0, 45.0, 0.0, 1.0, 1.0, 1.0));
            return null;
        });
        final double[] bMoved = onWorkspace(rule.getScenario(),
                (activity, workspace) -> transformValues());
        final long[] orderWithB = onWorkspace(rule.getScenario(),
                (activity, workspace) -> sceneOrder());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("three user acts, three steps",
                    3, NativeViewport.constructionUndoDepth());

            // 1. B's placement, and only B's.
            assertEquals(NativeViewport.HISTORY_OK, NativeViewport.constructionUndo());
            assertEquals("B goes back to the origin", 0.0,
                    transformValues()[0], 0.0);
            NativeViewport.sceneSelectBody(bodyA);
            assertArrayEquals("A is untouched by B's undo", aEdited, constructionState(), 0.0);
            NativeViewport.sceneSelectBody(bodyB);

            // 2. B's creation.
            assertEquals(NativeViewport.HISTORY_OK, NativeViewport.constructionUndo());
            assertEquals("B is gone", null, findBody(bodyB));
            assertArrayEquals("and A is still the edited cylinder",
                    aEdited, constructionState(), 0.0);

            // 3. A's shape.
            assertEquals(NativeViewport.HISTORY_OK, NativeViewport.constructionUndo());
            assertEquals("A returns to the baseline box",
                    NativeViewport.PRIMITIVE_BOX,
                    (int) constructionState()[0]);
            assertEquals(0, NativeViewport.constructionUndoDepth());
            assertEquals(3, NativeViewport.constructionRedoDepth());

            // And forward again, in the same order.
            assertEquals(NativeViewport.HISTORY_OK, NativeViewport.constructionRedo());
            assertArrayEquals("A's shape comes back first",
                    aEdited, constructionState(), 0.0);
            assertEquals(NativeViewport.HISTORY_OK, NativeViewport.constructionRedo());
            assertArrayEquals("then B, with the ordered ids intact",
                    orderWithB, sceneOrder());
            assertEquals(NativeViewport.HISTORY_OK, NativeViewport.constructionRedo());
            assertEquals("then B's placement", bodyB, NativeViewport.sceneActiveBodyId());
            assertArrayEquals(bMoved, transformValues(), 0.0);
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // S019-12, S019-13 — redo invalidation
    // -----------------------------------------------------------------------

    /** S019-12. A real new edit after an undo throws the forward branch away. */
    @Test
    public void s01912_aNewEditAfterAnUndoClearsTheRedo() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionSphere(1.25);
            NativeViewport.constructionUndo();
            assertTrue("precondition: there is something to redo",
                    NativeViewport.constructionRedoAvailable());

            NativeViewport.applyConstructionCone(1.0, 2.0);
            assertFalse("S019-12: a different real edit ends the old branch",
                    NativeViewport.constructionRedoAvailable());
            workspace.syncFromNative();
            assertFalse("and the control says so", workspace.redoAction().isEnabled());
            return null;
        });
    }

    /** S019-13. A refusal, a no-op and an empty transaction do NOT. */
    @Test
    public void s01913_aRejectedOrNoOpActionAfterAnUndoKeepsTheRedo() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionSphere(1.25);
            NativeViewport.constructionUndo();
            assertTrue("precondition", NativeViewport.constructionRedoAvailable());

            NativeViewport.applyConstructionBox(-3.0, 1.0, 1.0);
            assertTrue("a rejected Apply is not a new branch",
                    NativeViewport.constructionRedoAvailable());

            final double[] current = constructionState();
            NativeViewport.applyConstructionBox(current[NativeViewport.PRIMITIVE_BOX_WIDTH],
                    current[NativeViewport.PRIMITIVE_BOX_WIDTH + 1],
                    current[NativeViewport.PRIMITIVE_BOX_WIDTH + 2]);
            assertTrue("nor is an identical Apply",
                    NativeViewport.constructionRedoAvailable());

            assertTrue(NativeViewport.beginConstructionEdit());
            assertFalse("an edit that changed nothing records nothing",
                    NativeViewport.commitConstructionEdit());
            assertTrue("and does not end the branch either",
                    NativeViewport.constructionRedoAvailable());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // S019-14 — presentation is not history
    // -----------------------------------------------------------------------

    /** S019-14. Nothing the user can do that leaves the model alone writes a
     *  Construction history step. */
    @Test
    public void s01914_uiOnlyStateChangesWriteNoHistory() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionSphere(1.4);
            assertEquals("precondition: one real step to hold the baseline",
                    1, NativeViewport.constructionUndoDepth());
            return null;
        });

        // Selection, in both directions, through the scene control.
        addPrimitiveFromThePalette(R.id.add_primitive_cone);
        final long second = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneActiveBodyId());
        final int depth = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.constructionUndoDepth());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final long[] order = sceneOrder();
            NativeViewport.sceneSelectBody(order[0]);
            workspace.syncFromNative();
            assertEquals("selecting another body is not an edit",
                    depth, NativeViewport.constructionUndoDepth());
            NativeViewport.sceneSelectBody(second);
            workspace.syncFromNative();
            assertEquals("and neither is selecting back",
                    depth, NativeViewport.constructionUndoDepth());

            // The editing context, the display unit and the appearance.
            workspace.uiState().setConstructionTool(EditorUiState.CONSTRUCTION_TOOL_TRANSFORM);
            workspace.uiState().setDisplayUnit(LengthUnit.MILLIMETERS);
            workspace.syncFromNative();
            assertEquals("switching Shape/Transform is not an edit",
                    depth, NativeViewport.constructionUndoDepth());
            workspace.uiState().setConstructionTool(EditorUiState.CONSTRUCTION_TOOL_SHAPE);
            workspace.uiState().setDisplayUnit(LengthUnit.METERS);

            // Grid and the rest of the display settings.
            final boolean grid = NativeViewport.gridVisible();
            NativeViewport.setGridVisible(!grid);
            NativeViewport.setGridVisible(grid);
            NativeViewport.setShadingModel(NativeViewport.shadingModel());
            assertEquals("presentation never writes Construction history",
                    depth, NativeViewport.constructionUndoDepth());
            return null;
        });

        // The context surfaces, opened and closed the way a user opens them.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            openPrecision(workspace);
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            closePrecision(workspace);
            openObjectsPanel(workspace);
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            closeObjectsPanel(workspace);
            openAddPrimitive(workspace);
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            closeAddPrimitive(workspace);
            assertEquals("opening and closing a surface is not an edit",
                    depth, NativeViewport.constructionUndoDepth());
            workspace.setChromeHidden(true);
            workspace.setChromeHidden(false);
            assertEquals("and neither is hiding the chrome",
                    depth, NativeViewport.constructionUndoDepth());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // S019-15, S019-16, S019-17 — the transaction boundary Stage 020 needs
    // -----------------------------------------------------------------------

    /** S019-15. Begin, many transform updates, commit — one step, and the undo
     *  lands on the pre-drag state rather than on an intermediate frame. */
    @Test
    public void s01915_manyUpdatesInOneTransactionCommitAsOneStep() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] preDrag = transformValues();

            assertTrue("the edit opens", NativeViewport.beginConstructionEdit());
            assertFalse("and refuses to nest", NativeViewport.beginConstructionEdit());
            assertFalse("no step can be taken while one is open",
                    NativeViewport.constructionUndoAvailable());

            for (int step = 1; step <= 12; step++) {
                NativeViewport.applyBoxTransform(0.1 * step, 0.0, 0.0, 0.0, 3.0 * step, 0.0, 1.0, 1.0, 1.0);
                assertEquals("an update inside a transaction is not a step of its own",
                        0, NativeViewport.constructionUndoDepth());
            }
            final double[] finalDrag = transformValues();
            assertEquals("the live state followed every update", 1.2, finalDrag[0], 1.0e-9);

            assertTrue("the commit records the one step",
                    NativeViewport.commitConstructionEdit());
            assertEquals("S019-15: twelve updates, one history step",
                    1, NativeViewport.constructionUndoDepth());

            assertEquals(NativeViewport.HISTORY_OK, NativeViewport.constructionUndo());
            assertArrayEquals("undo lands on the pre-drag state, not on frame 11",
                    preDrag, transformValues(), 0.0);
            assertEquals(NativeViewport.HISTORY_OK, NativeViewport.constructionRedo());
            assertArrayEquals("redo lands on the final drag state",
                    finalDrag, transformValues(), 0.0);
            return null;
        });
    }

    /** S019-16. Cancel puts the pre-state back and records nothing. */
    @Test
    public void s01916_cancellingATransactionRestoresThePreStateAndRecordsNothing() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionSphere(1.5);
            final int depth = NativeViewport.constructionUndoDepth();
            final double[] preDrag = transformValues();
            final double[] preShape = constructionState();

            NativeViewport.beginConstructionEdit();
            for (int step = 1; step <= 5; step++) {
                NativeViewport.applyBoxTransform(0.0, 0.0, -0.4 * step, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
            }
            NativeViewport.cancelConstructionEdit();

            assertArrayEquals("S019-16: the placement returns to where the drag began",
                    preDrag, transformValues(), 0.0);
            assertArrayEquals("and nothing else moved", preShape, constructionState(), 0.0);
            assertEquals("S019-16: a cancelled edit is not a history step",
                    depth, NativeViewport.constructionUndoDepth());
            return null;
        });
    }

    /** S019-17. The history is bounded, and the oldest step is what falls out. */
    @Test
    public void s01917_theHistoryIsBoundedAndEvictsTheOldestFirst() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            // Deliberately more than any plausible capacity: the point is that
            // the depth STOPS growing and that undoing everything left does not
            // reach the state the evicted steps knew about.
            for (int i = 0; i < 220; i++) {
                NativeViewport.applyConstructionSphere(0.5 + 0.01 * i);
            }
            final int depth = NativeViewport.constructionUndoDepth();
            assertTrue("S019-17: the history is bounded, not unbounded — depth " + depth,
                    depth < 220);
            assertTrue("and it is not trivially small", depth >= 16);

            for (int i = 0; i < depth; i++) {
                assertEquals(NativeViewport.HISTORY_OK, NativeViewport.constructionUndo());
            }
            assertFalse("an exhausted stack stops cleanly",
                    NativeViewport.constructionUndoAvailable());
            assertEquals("S019-17: eviction is deterministic — the oldest steps are the "
                            + "ones gone, so the earliest reachable state is the one the "
                            + "retained window begins at, not the startup box",
                    NativeViewport.PRIMITIVE_SPHERE,
                    (int) constructionState()[0]);

            assertEquals("and every retained step is still redoable",
                    depth, NativeViewport.constructionRedoDepth());
            for (int i = 0; i < depth; i++) {
                assertEquals(NativeViewport.HISTORY_OK, NativeViewport.constructionRedo());
            }
            assertEquals(0.5 + 0.01 * 219,
                    constructionState()[NativeViewport.PRIMITIVE_SPHERE_DIAMETER], 1.0e-9);
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // S019-18, S019-19 — the Android lifecycle
    // -----------------------------------------------------------------------

    /** S019-18. A rotation keeps the history, and the rebuilt chrome reports it
     *  correctly. */
    @Test
    public void s01918_aRotationKeepsTheHistoryAndTheControlsAgree() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionSphere(1.75);
            NativeViewport.applyBoxTransform(2.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
            NativeViewport.constructionUndo();
            workspace.syncFromNative();
            assertTrue(workspace.undoAction().isEnabled());
            assertTrue(workspace.redoAction().isEnabled());
            return null;
        });

        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        waitForLayout(rule.getScenario(), true);
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("S019-18: the history is native and survives a rebuilt view",
                    1, NativeViewport.constructionUndoDepth());
            assertEquals(1, NativeViewport.constructionRedoDepth());
            assertTrue("and the rebuilt chrome reads native state rather than "
                            + "remembering its own", workspace.undoAction().isEnabled());
            assertTrue(workspace.redoAction().isEnabled());
            assertEquals("and it still works", NativeViewport.HISTORY_OK,
                    NativeViewport.constructionUndo());
            return null;
        });
    }

    /** S019-19. HOME and resume keep it too. */
    @Test
    public void s01919_homeAndResumeKeepTheInSessionHistory() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionCone(1.0, 2.0);
            NativeViewport.applyConstructionSphere(1.1);
            NativeViewport.constructionUndo();
            return null;
        });

        rule.getScenario().moveToState(androidx.lifecycle.Lifecycle.State.CREATED);
        settle();
        rule.getScenario().moveToState(androidx.lifecycle.Lifecycle.State.RESUMED);
        settleLayout();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("S019-19: both stacks survive a stop/resume",
                    1, NativeViewport.constructionUndoDepth());
            assertEquals(1, NativeViewport.constructionRedoDepth());
            assertTrue(workspace.undoAction().isEnabled());
            assertTrue(workspace.redoAction().isEnabled());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // S019-20..22, S019-24 — Construction history is not Sculpt undo
    // -----------------------------------------------------------------------

    /** S019-20, S019-24. Crossing the Construction/Sculpt seam writes no
     *  Construction step, and the Construction history is not reachable from
     *  Sculpt.
     *
     *  <p>Since {@code ARCH-OWNER-12} the two controls are DRAWN in Sculpt,
     *  because there they mean the Sculpt history — which is a different thing
     *  this test deliberately does not touch. What it still asserts is the part
     *  that did not change: the Construction entry points refuse in Sculpt, and
     *  the refusal costs the Construction stack nothing. */
    @Test
    public void s01920_theSculptSeamWritesNoHistoryAndWithdrawsTheControls() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionSphere(1.0);
            assertEquals("precondition: one Construction step exists",
                    1, NativeViewport.constructionUndoDepth());
            return null;
        });

        clickChrome(R.id.freeze_to_sculpt);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("precondition: the product is sculpting",
                    NativeViewport.MODE_SCULPT, NativeViewport.productMode());
            assertEquals("S019-20: Start Sculpting is navigation, not a Construction edit",
                    1, NativeViewport.constructionUndoDepth());
            assertEquals("S019-24: the pair is drawn in Sculpt too, where it means the "
                            + "Sculpt history", View.VISIBLE,
                    workspace.historyGroup().getVisibility());
            assertEquals("S019-24: and the guard below JNI stands whatever the chrome does",
                    NativeViewport.HISTORY_REFUSED_IN_SCULPT, NativeViewport.constructionUndo());
            assertEquals(NativeViewport.HISTORY_REFUSED_IN_SCULPT,
                    NativeViewport.constructionRedo());
            assertEquals("and the refusal changed nothing",
                    1, NativeViewport.constructionUndoDepth());
            return null;
        });

        clickChrome(R.id.back_to_construction);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("S019-20: and coming back is not an edit either",
                    1, NativeViewport.constructionUndoDepth());
            assertEquals("the pair returns in Construction", View.VISIBLE,
                    workspace.historyGroup().getVisibility());
            assertTrue(workspace.undoAction().isEnabled());
            return null;
        });
    }

    /** S019-21, S019-22. A real Grab stroke changes Sculpt state and no
     *  Construction history; a Construction undo afterwards leaves the sculpt
     *  mesh's identity alone except through the stale-source flag that already
     *  existed. */
    @Test
    public void s01921_aSculptStrokeIsNotConstructionHistory() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionSphere(1.0);
            NativeViewport.setSculptBrush(300.0, 1.0);
            return null;
        });
        clickChrome(R.id.freeze_to_sculpt);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.freezeToSculpt());
            NativeViewport.setSculptTool(NativeViewport.TOOL_GRAB);
            return null;
        });
        settleLayout();

        final int depthBeforeStroke = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.constructionUndoDepth());
        final double[] beforeStroke = onWorkspace(rule.getScenario(),
                (activity, workspace) -> sculptState());

        strokeTheModel();

        final double[] afterStroke = onWorkspace(rule.getScenario(),
                (activity, workspace) -> sculptState());
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertNotEquals("precondition: the stroke really edited the mesh",
                    beforeStroke[NativeViewport.SCULPT_REVISION],
                    afterStroke[NativeViewport.SCULPT_REVISION]);
            assertEquals("S019-21: a sculpt stroke writes no Construction history",
                    depthBeforeStroke, NativeViewport.constructionUndoDepth());
            return null;
        });

        clickChrome(R.id.back_to_construction);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("precondition: there is a Construction step to take back",
                    NativeViewport.constructionUndoAvailable());
            assertEquals(NativeViewport.HISTORY_OK, NativeViewport.constructionUndo());

            final double[] afterUndo = sculptState();
            assertEquals("S019-22: the sculpt mesh's revision is untouched",
                    afterStroke[NativeViewport.SCULPT_REVISION],
                    afterUndo[NativeViewport.SCULPT_REVISION], 0.0);
            assertEquals("S019-22: and its vertex count",
                    afterStroke[NativeViewport.SCULPT_VERTEX_COUNT],
                    afterUndo[NativeViewport.SCULPT_VERTEX_COUNT], 0.0);
            assertEquals("S019-22: and its index count",
                    afterStroke[NativeViewport.SCULPT_INDEX_COUNT],
                    afterUndo[NativeViewport.SCULPT_INDEX_COUNT], 0.0);
            assertEquals("S019-22: and its stroke count",
                    afterStroke[NativeViewport.SCULPT_STROKE_COUNT],
                    afterUndo[NativeViewport.SCULPT_STROKE_COUNT], 0.0);
            assertEquals("S019-22: and its identity",
                    afterStroke[NativeViewport.SCULPT_OBJECT_ID],
                    afterUndo[NativeViewport.SCULPT_OBJECT_ID], 0.0);
            assertTrue("S019-22: the ONE thing that moves is the already-existing "
                            + "stale-source flag, because the Construction Source did "
                            + "change under the frozen mesh",
                    afterUndo[NativeViewport.SCULPT_SOURCE_STALE] != 0.0);

            assertEquals(NativeViewport.HISTORY_OK, NativeViewport.constructionRedo());
            assertEquals("and a redo is no different",
                    afterStroke[NativeViewport.SCULPT_REVISION],
                    sculptState()[NativeViewport.SCULPT_REVISION], 0.0);
            return null;
        });

        clickChrome(R.id.resume_sculpt);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("Resume Sculpt returns the SAME mesh — nothing here acted as "
                            + "a Sculpt undo", afterStroke[NativeViewport.SCULPT_REVISION],
                    sculptState()[NativeViewport.SCULPT_REVISION], 0.0);
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // S019-23, S019-25 — the controls themselves
    // -----------------------------------------------------------------------

    /** S019-23. The drawn state is native state, in whichever window this run
     *  is laid out in. */
    @Test
    public void s01923_theControlsEnabledStateIsAlwaysNativeState() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEnabledStateMatchesNative(workspace, "an empty history");
            NativeViewport.applyConstructionSphere(1.2);
            workspace.syncFromNative();
            assertEnabledStateMatchesNative(workspace, "after one edit");
            NativeViewport.constructionUndo();
            workspace.syncFromNative();
            assertEnabledStateMatchesNative(workspace, "after undoing it");
            NativeViewport.constructionRedo();
            workspace.syncFromNative();
            assertEnabledStateMatchesNative(workspace, "after redoing it");
            return null;
        });

        // And after a rotation, which rebuilds the whole workspace.
        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        waitForLayout(rule.getScenario(), true);
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEnabledStateMatchesNative(workspace, "after a rotation");
            return null;
        });
    }

    /** S019-25. Both controls meet the 48 dp interactive floor, are laid out
     *  inside the window, and stand clear of every other chrome surface. */
    @Test
    public void s01925_theControlsMeetTheTouchFloorAndOverlapNothing() {
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int floor = Math.round(TypedValue.applyDimension(
                    TypedValue.COMPLEX_UNIT_DIP, 48.0f,
                    workspace.getResources().getDisplayMetrics()));

            for (View control : new View[]{workspace.undoAction(), workspace.redoAction()}) {
                assertTrue("a history control must be on screen to be pressed",
                        control.getVisibility() == View.VISIBLE && control.getWidth() > 0);
                assertTrue("S019-25: the HIT AREA is at least 48 dp wide — "
                                + control.getWidth() + " < " + floor,
                        control.getWidth() >= floor);
                assertTrue("S019-25: and at least 48 dp tall — "
                                + control.getHeight() + " < " + floor,
                        control.getHeight() >= floor);
                assertTrue("S019-25: and it is laid out inside the window",
                        WorkspaceTestSupport.isFullyOnScreen(control, workspace));
            }

            // Nothing else may sit under them. The union arithmetic already
            // counts the history capsule as chrome; what matters here is that it
            // does not INTERSECT another surface, because z-order is not a fix
            // for a control standing on another control.
            final Rect history = rectOf(workspace, workspace.historyGroup());
            for (Rect other : workspace.chromeRects()) {
                if (other.equals(history)) {
                    continue;
                }
                assertFalse("S019-25: the history capsule stands clear of every other "
                                + "chrome surface — " + history + " vs " + other,
                        Rect.intersects(history, other));
            }
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // S019-26 — publication discipline
    // -----------------------------------------------------------------------

    /** S019-26. Wrapping the existing Apply paths in a transaction added no
     *  geometry work: one shape Apply is still one publication, a commit is
     *  none, and an undo publishes once. */
    @Test
    public void s01926_transactionWrappingAddsNoDuplicatePublication() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final long start = NativeViewport.constructionMeshRevision();
            assertEquals(NativeViewport.APPLY_APPLIED,
                    NativeViewport.applyConstructionSphere(1.6));
            assertEquals("S019-26: one Apply is still exactly one publication",
                    start + 1, NativeViewport.constructionMeshRevision());

            final long afterApply = NativeViewport.constructionMeshRevision();
            assertEquals("S019-26: an undo publishes exactly once",
                    afterApply + 1, undoAndReadRevision());
            assertEquals("S019-26: and so does the redo",
                    afterApply + 2, redoAndReadRevision());

            // The synthetic drag: updates publish live, the commit publishes
            // nothing at all because the mutation already produced final state.
            final long beforeDrag = NativeViewport.constructionMeshRevision();
            NativeViewport.beginConstructionEdit();
            for (int step = 1; step <= 6; step++) {
                NativeViewport.applyBoxTransform(0.1 * step, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
            }
            final long beforeCommit = NativeViewport.constructionMeshRevision();
            assertEquals("a placement drag publishes no geometry at all",
                    beforeDrag, beforeCommit);
            NativeViewport.commitConstructionEdit();
            assertEquals("S019-26: and the commit itself adds no publication",
                    beforeCommit, NativeViewport.constructionMeshRevision());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // Support
    // -----------------------------------------------------------------------

    private static void assertEnabledStateMatchesNative(EditorWorkspaceView workspace,
                                                        String when) {
        assertEquals("S019-23: Undo is drawn enabled exactly when native canUndo says so, "
                        + when, NativeViewport.constructionUndoAvailable(),
                workspace.undoAction().isEnabled());
        assertEquals("S019-23: and Redo exactly when native canRedo says so, " + when,
                NativeViewport.constructionRedoAvailable(),
                workspace.redoAction().isEnabled());
    }

    private static Rect rectOf(EditorWorkspaceView workspace, View surface) {
        final Rect rect = new Rect(0, 0, surface.getWidth(), surface.getHeight());
        workspace.offsetDescendantRectToMyCoords(surface, rect);
        return rect;
    }

    private long undoAndReadRevision() {
        NativeViewport.constructionUndo();
        return NativeViewport.constructionMeshRevision();
    }

    private long redoAndReadRevision() {
        NativeViewport.constructionRedo();
        return NativeViewport.constructionMeshRevision();
    }

    /** Presses one global chrome control by its stable id. */
    private void clickChrome(final int id) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(id).performClick();
            return null;
        });
        settleLayout();
    }

    /** Presses Undo the way the user does, rather than calling into native code. */
    private void undoFromTheControl() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("the control must be live for the press to mean anything",
                    workspace.undoAction().isEnabled());
            workspace.undoAction().performClick();
            return null;
        });
        settleLayout();
    }

    private void redoFromTheControl() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue(workspace.redoAction().isEnabled());
            workspace.redoAction().performClick();
            return null;
        });
        settleLayout();
    }

    /** Creates a body by choosing a shape from the palette, the way a user does. */
    private void addPrimitiveFromThePalette(final int tileId) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            openAddPrimitive(workspace);
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.addPrimitivePalette().findViewById(tileId).performClick();
            return null;
        });
        settleLayout();
    }

    /** Drives a real Grab stroke through the native touch path. */
    private void strokeTheModel() {
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

    private static double[] sculptState() {
        final double[] state = new double[NativeViewport.SCULPT_STATE_SIZE];
        NativeViewport.sculptState(state);
        return state;
    }

    /** The Construction Source of the ACTIVE body: the exact primitive and its
     *  placement, which together are what a history step restores. */
    private static double[] constructionState() {
        final double[] primitive = new double[NativeViewport.PRIMITIVE_STATE_SIZE];
        final double[] transform = new double[NativeViewport.TRANSFORM_SIZE];
        NativeViewport.constructionPrimitive(primitive);
        NativeViewport.boxTransform(transform);
        final double[] all = new double[primitive.length + transform.length];
        System.arraycopy(primitive, 0, all, 0, primitive.length);
        System.arraycopy(transform, 0, all, primitive.length, transform.length);
        return all;
    }

    private static double[] transformValues() {
        final double[] transform = new double[NativeViewport.TRANSFORM_SIZE];
        NativeViewport.boxTransform(transform);
        return transform;
    }

    private static long[] sceneOrder() {
        final long[] ids = new long[NativeViewport.sceneBodyCount()];
        NativeViewport.sceneBodyIds(ids);
        return ids;
    }

    private static Long findBody(long objectId) {
        for (long id : sceneOrder()) {
            if (id == objectId) {
                return id;
            }
        }
        return null;
    }
}
