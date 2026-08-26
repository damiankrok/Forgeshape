package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.closePrecision;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.openPrecision;
import static com.forgeshape.app.WorkspaceTestSupport.releaseOrientation;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.selectConstructionTool;
import static com.forgeshape.app.WorkspaceTestSupport.setOrientation;
import static com.forgeshape.app.WorkspaceTestSupport.settle;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.content.pm.ActivityInfo;
import android.graphics.Rect;
import android.os.SystemClock;
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
 * Stage 020: the Construction Move / Rotate gizmo, from the product side of JNI.
 *
 * <p>The solvers themselves — closest approach, the plane fallback, the signed
 * angle, the unwrap, accumulation past a full turn, the screen-constant scale
 * and the transaction boundary — are proved against an isolated scene by the
 * native {@code FORGESHAPE_GIZMO_SELFTEST} suite, which is where they belong: a
 * domain rule that can only be shown through an Android view is a rule nothing
 * else can rely on. What this suite adds is everything that is genuinely about
 * the product — that the workspace decides WHEN there is a gizmo and nothing
 * else, that a real {@link MotionEvent} through the real SurfaceView reaches the
 * real handle, that a drag arbitrates correctly against the camera, that one
 * gesture is one history step whatever the sample count, and that a transform
 * drag costs the mesh nothing.
 *
 * <p><b>No case writes a screen coordinate down.</b> Where a handle is on screen
 * is a live function of the camera and the window, so every drag starts from the
 * pixel native code reports for that handle on this run, and every chrome
 * control is found by its semantic id.
 */
@RunWith(AndroidJUnit4.class)
public final class EditorWorkspaceGizmoTest {

    /** How close two doubles have to be to be the same placement value. */
    private static final double EXACT = 0.0;

    /** A displacement below this is indistinguishable from a tap. */
    private static final double MOVED = 1e-6;

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
    // S020-PRE — the session-initialization boundary
    // -----------------------------------------------------------------------

    /**
     * S020-PRE-01 / S020-PRE-03. Answering the start question — either way —
     * leaves the Construction history empty.
     *
     * <p>The Sculpt answer is the one that matters. It reaches a sculptable mesh
     * by driving the real Construction entry points, which shapes the body into
     * a sphere; recorded, that would be the user's first Undo, and undoing it
     * would rewind a decision they never made. The production boundary is what
     * makes it not one, and this is what says so from the product side.
     */
    @Test
    public void s020pre0103_answeringTheStartQuestionLeavesAnEmptyHistory() {
        for (final boolean sculpt : new boolean[] {false, true}) {
            final int[] depths = onWorkspace(rule.getScenario(), (activity, workspace) -> {
                NativeViewport.debugResetConstructionHistory();
                workspace.showStartChooserAsFirstLaunch();
                workspace.findViewById(sculpt ? R.id.start_option_sculpt
                        : R.id.start_option_construction).performClick();
                return new int[] {NativeViewport.constructionUndoDepth(),
                        NativeViewport.constructionRedoDepth()};
            });
            settle();
            assertEquals("a new session must have nothing to undo (sculpt=" + sculpt + ")",
                    0, depths[0]);
            assertEquals("a new session must have nothing to redo (sculpt=" + sculpt + ")",
                    0, depths[1]);
            resetToBaselineConstruction(rule.getScenario());
        }
    }

    /**
     * S020-PRE-02. The direct-Sculpt startup path is production, reachable from
     * the start question, and is therefore bracketed rather than merely
     * documented.
     *
     * <p>Stated as an assertion about the CONTROL, so a future change that hides
     * the Sculpt answer, or that stops it seeding a sphere, fails here rather
     * than silently making the bracket dead code.
     */
    @Test
    public void s020pre02_theDirectSculptStartupPathIsProductionReachable() {
        final boolean[] facts = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.showStartChooserAsFirstLaunch();
            final View sculptAnswer = workspace.findViewById(R.id.start_option_sculpt);
            return new boolean[] {
                    sculptAnswer != null,
                    sculptAnswer != null && sculptAnswer.getVisibility() == View.VISIBLE,
                    sculptAnswer != null && sculptAnswer.isClickable()};
        });
        assertTrue("the Sculpt answer exists in the shipped chooser", facts[0]);
        assertTrue("and is on screen", facts[1]);
        assertTrue("and a user can press it", facts[2]);

        // It really does seed a Construction change, which is the whole reason
        // the boundary has to exist.
        final int kindAfter = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.start_option_sculpt).performClick();
            final double[] primitive = new double[NativeViewport.PRIMITIVE_STATE_SIZE];
            NativeViewport.constructionPrimitive(primitive);
            return (int) primitive[NativeViewport.PRIMITIVE_KIND];
        });
        settle();
        assertEquals("starting in Sculpt shapes the body into a sphere",
                NativeViewport.PRIMITIVE_SPHERE, kindAfter);
    }

    /**
     * S020-PRE-04. Coming back from a session that started in Sculpt reveals no
     * initialization step.
     */
    @Test
    public void s020pre04_directSculptThenBackRevealsNoInitialisationStep() {
        final int[] depths = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.debugResetConstructionHistory();
            workspace.showStartChooserAsFirstLaunch();
            workspace.findViewById(R.id.start_option_sculpt).performClick();
            NativeViewport.enterConstructionMode();
            workspace.syncFromNative();
            return new int[] {NativeViewport.constructionUndoDepth(),
                    NativeViewport.constructionRedoDepth()};
        });
        settle();
        assertEquals("Back to Construction reveals no seeded step", 0, depths[0]);
        assertEquals("and nothing to redo either", 0, depths[1]);
    }

    /**
     * S020-PRE-05. The bracket is scoped to the seed: a genuine user edit made
     * after it survives a rotation and a resume exactly as it did before.
     */
    @Test
    public void s020pre05_realHistorySurvivesRotationAndResumeAfterSeeding() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.debugResetConstructionHistory();
            workspace.showStartChooserAsFirstLaunch();
            workspace.findViewById(R.id.start_option_construction).performClick();
            NativeViewport.applyBoxTransform(1.5, 0.0, 0.0, 0.0, 0.0, 0.0);
            return null;
        });
        settle();
        assertEquals("the first user act is the first step", 1, undoDepth());

        setOrientation(rule.getScenario(), ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        settleLayout();
        assertEquals("a rotation keeps it", 1, undoDepth());

        rule.getScenario().moveToState(androidx.lifecycle.Lifecycle.State.CREATED);
        rule.getScenario().moveToState(androidx.lifecycle.Lifecycle.State.RESUMED);
        settle();
        assertEquals("a resume keeps it", 1, undoDepth());
    }

    // -----------------------------------------------------------------------
    // S020-01..05 — visibility and mode
    // -----------------------------------------------------------------------

    /** S020-01. The Shape context draws no handles and no mode selector. */
    @Test
    public void s02001_shapeContextHidesTheGizmo() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            selectConstructionTool(workspace, R.id.tool_rail_shape);
            return null;
        });
        settleLayout();
        assertFalse("no gizmo in the Shape context", gizmoVisible());
        assertEquals("and no mode selector", View.GONE, transformSelectorVisibility());
    }

    /** S020-02. Transform with a body draws both. */
    @Test
    public void s02002_transformContextShowsTheGizmoForTheActiveBody() {
        enterTransform();
        assertTrue("a body in Transform has handles", gizmoVisible());
        assertEquals("and the selector that chooses which kind",
                View.VISIBLE, transformSelectorVisibility());
        assertEquals("entering Transform selects Move", NativeViewport.GIZMO_MODE_MOVE, gizmoMode());
    }

    /**
     * S020-03. Sculpt has neither.
     *
     * <p>The control is absent AND the guard below JNI still refuses, because
     * removing a control is not removing a guard.
     */
    @Test
    public void s02003_sculptHidesTheGizmoAndItsControls() {
        enterTransform();
        final boolean frozen = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final boolean ok = NativeViewport.freezeToSculpt() == NativeViewport.SCULPT_OK;
            workspace.syncFromNative();
            return ok;
        });
        settleLayout();
        assertTrue("precondition: the body froze", frozen);
        assertFalse("no handles while sculpting", gizmoVisible());
        assertEquals("and no mode selector", View.GONE, transformSelectorVisibility());

        // The guard stays even though nothing offers the act.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.setGizmoActive(true);
            return null;
        });
        assertFalse("native code refuses a gizmo while sculpting", gizmoState()[
                NativeViewport.GIZMO_ACTIVE] != 0.0);

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.enterConstructionMode();
            workspace.syncFromNative();
            return null;
        });
        settle();
    }

    /** S020-04. Selecting another body retargets the pivot and records nothing. */
    @Test
    public void s02004_changingActiveBodyRetargetsWithoutHistory() {
        enterTransform();
        final long first = NativeViewport.sceneActiveBodyId();
        final float[] firstPivot = pivotPixel();

        final long second = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final long created = NativeViewport.sceneAddBody();
            NativeViewport.applyBoxTransform(3.0, 0.0, 0.0, 0.0, 0.0, 0.0);
            workspace.syncFromNative();
            return created;
        });
        settle();
        final int depthAfterCreation = undoDepth();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.sceneSelectBody(first);
            workspace.syncFromNative();
            return null;
        });
        settle();
        assertEquals("selection is not an edit", depthAfterCreation, undoDepth());
        assertNotEquals("the two bodies are different", first, second);

        final float[] backPivot = pivotPixel();
        assertEquals("the pivot returned to the first body's placement",
                firstPivot[0], backPivot[0], 1.0f);
        assertEquals(firstPivot[1], backPivot[1], 1.0f);
    }

    /** S020-05. Switching Move to Rotate costs neither a step nor a revision. */
    @Test
    public void s02005_modeSelectorCreatesNoHistoryAndNoPublication() {
        enterTransform();
        final int depthBefore = undoDepth();
        final long revisionBefore = NativeViewport.constructionMeshRevision();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.transformRotateAction().performClick();
            return null;
        });
        settle();
        assertEquals("Rotate is held", NativeViewport.GIZMO_MODE_ROTATE, gizmoMode());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.transformMoveAction().performClick();
            return null;
        });
        settle();
        assertEquals("Move is held again", NativeViewport.GIZMO_MODE_MOVE, gizmoMode());

        assertEquals("switching mode is not an edit", depthBefore, undoDepth());
        assertEquals("and publishes no geometry",
                revisionBefore, NativeViewport.constructionMeshRevision());
    }

    // -----------------------------------------------------------------------
    // S020-06..13 — Move
    // -----------------------------------------------------------------------

    /** S020-06/07/08/09. Each axis moves its own coordinate and nothing else. */
    @Test
    public void s02006to09_eachMoveAxisChangesOnlyItsOwnCoordinate() {
        final int[] axes = {NativeViewport.GIZMO_AXIS_X, NativeViewport.GIZMO_AXIS_Y,
                NativeViewport.GIZMO_AXIS_Z};
        for (int i = 0; i < axes.length; i++) {
            resetToBaselineConstruction(rule.getScenario());
            enterTransform();
            final double[] before = placement();
            final boolean dragged = dragHandle(axes[i], 1.0f, 12);
            final double[] after = placement();
            assertTrue("the " + axisName(axes[i]) + " handle was grabbed", dragged);

            for (int slot = 0; slot < 6; slot++) {
                final boolean isTheDraggedPosition = slot == i;
                if (isTheDraggedPosition) {
                    assertTrue("dragging " + axisName(axes[i]) + " must move its coordinate",
                            Math.abs(after[slot] - before[slot]) > MOVED);
                } else {
                    assertEquals("dragging " + axisName(axes[i]) + " must not touch slot " + slot,
                            before[slot], after[slot], EXACT);
                }
            }
        }
    }

    /**
     * S020-10. An axis pointing nearly at the viewer stays finite.
     *
     * <p>The camera is orbited until the X axis is close to the view direction,
     * which is the case both solver paths degenerate in. What must never happen
     * is a NaN, an infinity, or a placement that leaps.
     */
    @Test
    public void s02010_aNearCameraParallelAxisStaysFinite() {
        enterTransform();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            // Straight down the world X axis. Driven through the camera's own
            // debug pose rather than by a synthetic orbit, so the case is about
            // the solver and not about gesture arithmetic.
            NativeViewport.debugSetCameraPose((float) (Math.PI * 0.5), 0.02f, 8.0f);
            return null;
        });
        settle();
        final double[] before = placement();
        dragHandle(NativeViewport.GIZMO_AXIS_X, 1.0f, 16);
        final double[] after = placement();
        for (int slot = 0; slot < 6; slot++) {
            assertTrue("slot " + slot + " must stay finite", isFinite(after[slot]));
            assertTrue("slot " + slot + " must not leap",
                    Math.abs(after[slot] - before[slot]) < 1000.0);
        }
    }

    /** S020-11. A tap on a handle records nothing. */
    @Test
    public void s02011_aTapOnAHandleCreatesNoHistory() {
        enterTransform();
        final int depthBefore = undoDepth();
        final double[] before = placement();
        final boolean grabbed = dragHandle(NativeViewport.GIZMO_AXIS_Y, 0.0f, 4);
        assertTrue("the handle was still grabbed", grabbed);
        assertEquals("a tap is not an edit", depthBefore, undoDepth());
        assertArrayExactlyEquals("and moved nothing", before, placement());
    }

    /** S020-12. A drag is one step no matter how many MOVE samples it carried. */
    @Test
    public void s02012_oneDragIsOneStepWhateverTheSampleCount() {
        enterTransform();
        final int depthBefore = undoDepth();
        final boolean dragged = dragHandle(NativeViewport.GIZMO_AXIS_X, 1.0f, 48);
        assertTrue("the handle was grabbed", dragged);
        assertEquals("forty-eight samples are one step", depthBefore + 1, undoDepth());
        assertTrue("and they really were many samples",
                gizmoState()[NativeViewport.GIZMO_DRAG_UPDATES] > 4.0);
    }

    /** S020-13. A cancelled drag restores exactly and records nothing. */
    @Test
    public void s02013_cancelRestoresTheExactPreDragPlacement() {
        enterTransform();
        final int depthBefore = undoDepth();
        final double[] before = placement();

        final double[] midDrag = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = viewportOf(workspace);
            final float[] start = handlePixel(NativeViewport.GIZMO_AXIS_X);
            final float[] step = screenStepAlong(NativeViewport.GIZMO_AXIS_X, 1.0f);
            final long when = SystemClock.uptimeMillis();
            sendFinger(viewport, when, when, MotionEvent.ACTION_DOWN, start[0], start[1]);
            for (int i = 1; i <= 6; i++) {
                sendFinger(viewport, when, when + 16L * i, MotionEvent.ACTION_MOVE,
                        start[0] + step[0] * i, start[1] + step[1] * i);
            }
            final double[] moved = readPlacement();
            sendFinger(viewport, when, when + 200L, MotionEvent.ACTION_CANCEL,
                    start[0] + step[0] * 6, start[1] + step[1] * 6);
            return moved;
        });
        settle();

        assertTrue("precondition: the drag had actually moved the body",
                Math.abs(midDrag[0] - before[0]) > MOVED);
        assertArrayExactlyEquals("cancel restores exactly", before, placement());
        assertEquals("and records nothing", depthBefore, undoDepth());
    }

    // -----------------------------------------------------------------------
    // S020-14..22 — Rotate
    // -----------------------------------------------------------------------

    /** S020-14/15/16/17. Each ring turns its own component and leaves position. */
    @Test
    public void s02014to17_eachRingChangesOnlyItsOwnRotation() {
        final int[] axes = {NativeViewport.GIZMO_AXIS_X, NativeViewport.GIZMO_AXIS_Y,
                NativeViewport.GIZMO_AXIS_Z};
        for (int i = 0; i < axes.length; i++) {
            resetToBaselineConstruction(rule.getScenario());
            enterTransform();
            selectRotate();
            final double[] before = placement();
            final boolean dragged = dragHandle(axes[i], 1.0f, 12);
            assertTrue("the " + axisName(axes[i]) + " ring was grabbed", dragged);
            final double[] after = placement();

            for (int slot = 0; slot < 6; slot++) {
                if (slot == 3 + i) {
                    assertTrue("the " + axisName(axes[i]) + " ring must turn its own component",
                            Math.abs(after[slot] - before[slot]) > MOVED);
                } else {
                    assertEquals("the " + axisName(axes[i]) + " ring must not touch slot " + slot,
                            before[slot], after[slot], EXACT);
                }
            }
        }
    }

    /**
     * S020-18/19/20. A long ring drag crosses half a turn and passes a whole
     * one without a jump, and an edge-on ring never produces a leap.
     *
     * <p>Continuity is asserted as "no sample moved the rotation by more than a
     * quarter turn", which is exactly what a ±180 wrap defect would violate —
     * and it is checked per sample rather than only at the end, because a drag
     * that jumped forward and back would pass an endpoint check.
     */
    @Test
    public void s02018to20_aLongRingDragIsContinuousAndPassesAFullTurn() {
        enterTransform();
        selectRotate();

        final double[] extremes = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = viewportOf(workspace);
            final float[] pivot = pivotPixelUnchecked();
            final float[] handle = handlePixel(NativeViewport.GIZMO_AXIS_Y);
            final float radiusX = handle[0] - pivot[0];
            final float radiusY = handle[1] - pivot[1];
            final long when = SystemClock.uptimeMillis();
            sendFinger(viewport, when, when, MotionEvent.ACTION_DOWN, handle[0], handle[1]);

            double previous = readPlacement()[4];
            double biggestStep = 0.0;
            double travelled = 0.0;
            // Three hundred small steps: a full turn and a quarter, driven
            // around the projected ring rather than in a straight line, so this
            // exercises the unwrap three hundred times.
            final int steps = 300;
            for (int i = 1; i <= steps; i++) {
                final double angle = 2.0 * Math.PI * 1.25 * i / steps;
                final float c = (float) Math.cos(angle);
                final float s = (float) Math.sin(angle);
                final float x = pivot[0] + radiusX * c - radiusY * s;
                final float y = pivot[1] + radiusY * c + radiusX * s;
                sendFinger(viewport, when, when + 8L * i, MotionEvent.ACTION_MOVE, x, y);
                final double now = readPlacement()[4];
                biggestStep = Math.max(biggestStep, Math.abs(now - previous));
                travelled = Math.abs(now);
                previous = now;
            }
            sendFinger(viewport, when, when + 8L * (steps + 1), MotionEvent.ACTION_UP,
                    handle[0], handle[1]);
            return new double[] {biggestStep, travelled};
        });
        settle();

        assertTrue("no single sample may jump a quarter turn (was " + extremes[0] + ")",
                extremes[0] < 90.0);
        assertTrue("a turn and a quarter must accumulate past 360 (was " + extremes[1] + ")",
                extremes[1] > 360.0);
        assertTrue("and must not be canonicalised back into a single turn",
                Math.abs(placement()[4]) > 360.0);
    }

    /** S020-21/22. A ring drag commits one step; a cancelled one restores. */
    @Test
    public void s02021and22_aRingDragCommitsOneStepAndCancelsExactly() {
        enterTransform();
        selectRotate();
        final int depthBefore = undoDepth();
        assertTrue("the ring was grabbed", dragHandle(NativeViewport.GIZMO_AXIS_Y, 1.0f, 24));
        assertEquals("one ring drag is one step", depthBefore + 1, undoDepth());

        final double[] before = placement();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = viewportOf(workspace);
            final float[] start = handlePixel(NativeViewport.GIZMO_AXIS_Y);
            final float[] step = screenTangentAt(NativeViewport.GIZMO_AXIS_Y, 1.0f);
            final long when = SystemClock.uptimeMillis();
            sendFinger(viewport, when, when, MotionEvent.ACTION_DOWN, start[0], start[1]);
            for (int i = 1; i <= 8; i++) {
                sendFinger(viewport, when, when + 16L * i, MotionEvent.ACTION_MOVE,
                        start[0] + step[0] * i, start[1] + step[1] * i);
            }
            sendFinger(viewport, when, when + 200L, MotionEvent.ACTION_CANCEL,
                    start[0] + step[0] * 8, start[1] + step[1] * 8);
            return null;
        });
        settle();
        assertArrayExactlyEquals("a cancelled ring drag restores exactly", before, placement());
        assertEquals("and records nothing", depthBefore + 1, undoDepth());
    }

    // -----------------------------------------------------------------------
    // S020-23..28 — history and exact-value coherence
    // -----------------------------------------------------------------------

    /** S020-23/24. Undo goes back to before the drag; redo goes forward to it. */
    @Test
    public void s02023and24_undoAndRedoBracketOneDrag() {
        enterTransform();
        final double[] before = placement();
        assertTrue(dragHandle(NativeViewport.GIZMO_AXIS_X, 1.0f, 10));
        final double[] afterMove = placement();

        selectRotate();
        assertTrue(dragHandle(NativeViewport.GIZMO_AXIS_Y, 1.0f, 10));
        final double[] afterRotate = placement();

        undo();
        assertArrayExactlyEquals("undo returns to the move result", afterMove, placement());
        undo();
        assertArrayExactlyEquals("and again to before the move", before, placement());
        redo();
        assertArrayExactlyEquals("redo returns the move", afterMove, placement());
        redo();
        assertArrayExactlyEquals("and the rotate", afterRotate, placement());
    }

    /** S020-25. The exact values opened after a drag read the native result. */
    @Test
    public void s02025_exactTransformReadsTheGizmoResult() {
        enterTransform();
        assertTrue(dragHandle(NativeViewport.GIZMO_AXIS_X, 1.0f, 12));
        final double[] native_ = placement();

        final double[] shown = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            openPrecision(workspace);
            return readPlacementEditorValues(workspace);
        });
        settleLayout();
        assertEquals("the exact Position X is the gizmo's own result",
                native_[0], shown[0], 1e-4);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            closePrecision(workspace);
            return null;
        });
        settleLayout();
    }

    /** S020-26. An exact Apply moves the handles immediately. */
    @Test
    public void s02026_exactApplyRepositionsTheGizmo() {
        enterTransform();
        final float[] before = pivotPixel();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyBoxTransform(2.5, 1.25, -0.75, 0.0, 0.0, 0.0);
            workspace.syncFromNative();
            return null;
        });
        settle();
        final float[] after = pivotPixel();
        assertTrue("the pivot followed the typed placement",
                Math.hypot(after[0] - before[0], after[1] - before[1]) > 4.0);
    }

    /** S020-27. A new gizmo edit after an undo throws the redo away. */
    @Test
    public void s02027_aNewDragInvalidatesTheRedo() {
        enterTransform();
        assertTrue(dragHandle(NativeViewport.GIZMO_AXIS_X, 1.0f, 10));
        undo();
        assertTrue("there is something to redo", NativeViewport.constructionRedoAvailable());
        assertTrue(dragHandle(NativeViewport.GIZMO_AXIS_Y, 1.0f, 10));
        assertFalse("a new edit is a new branch", NativeViewport.constructionRedoAvailable());
    }

    /** S020-28. Two bodies undo in chronological order, each on its own. */
    @Test
    public void s02028_twoBodiesUndoChronologicallyAndIndependently() {
        enterTransform();
        final long first = NativeViewport.sceneActiveBodyId();
        assertTrue(dragHandle(NativeViewport.GIZMO_AXIS_X, 1.0f, 10));
        final double firstX = placement()[0];

        final long second = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final long created = NativeViewport.sceneAddBody();
            workspace.syncFromNative();
            return created;
        });
        settle();
        assertTrue(dragHandle(NativeViewport.GIZMO_AXIS_Y, 1.0f, 10));
        final double secondY = placement()[1];

        undo();  // the second body's drag
        assertEquals("the newest edit undoes first", 0.0, placementOf(second)[1], EXACT);
        assertEquals("and the other body is untouched", firstX, placementOf(first)[0], EXACT);
        redo();
        assertEquals("redo puts it back", secondY, placementOf(second)[1], EXACT);
        assertNotEquals("and still only that body moved", 0.0, firstX);
    }

    // -----------------------------------------------------------------------
    // S020-29..34 — input arbitration
    // -----------------------------------------------------------------------

    /**
     * S020-29/30. A pointer on a handle drags the body and leaves the camera
     * alone; a pointer anywhere else navigates exactly as before.
     */
    @Test
    public void s02029and30_theGizmoTakesOnlyTheGesturesThatStartOnAHandle() {
        enterTransform();
        final float[] cameraBefore = cameraPose();
        assertTrue(dragHandle(NativeViewport.GIZMO_AXIS_X, 1.0f, 12));
        final float[] cameraAfterHandleDrag = cameraPose();
        assertEquals("a captured handle must not orbit", cameraBefore[0],
                cameraAfterHandleDrag[0], 1e-4f);
        assertEquals(cameraBefore[1], cameraAfterHandleDrag[1], 1e-4f);

        final double[] placementBefore = placement();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = viewportOf(workspace);
            // A corner of the viewport: far from every handle, and the hit test
            // is asked rather than assumed, so this cannot silently start
            // landing on one.
            final float x = viewport.getWidth() * 0.12f;
            final float y = viewport.getHeight() * 0.14f;
            assertEquals("precondition: this pixel is not a handle",
                    NativeViewport.GIZMO_AXIS_NONE, NativeViewport.gizmoHitTest(x, y));
            final long when = SystemClock.uptimeMillis();
            sendFinger(viewport, when, when, MotionEvent.ACTION_DOWN, x, y);
            for (int i = 1; i <= 8; i++) {
                sendFinger(viewport, when, when + 16L * i, MotionEvent.ACTION_MOVE,
                        x + i * 9.0f, y + i * 5.0f);
            }
            sendFinger(viewport, when, when + 200L, MotionEvent.ACTION_UP,
                    x + 72.0f, y + 40.0f);
            return null;
        });
        settle();
        final float[] cameraAfterOrbit = cameraPose();
        assertNotEquals("a drag off the handles still orbits",
                cameraAfterHandleDrag[0], cameraAfterOrbit[0], 1e-4f);
        assertArrayExactlyEquals("and moves no body", placementBefore, placement());
    }

    /** S020-31. ACTION_CANCEL closes the transaction. Covered by S020-13's
     *  restoration check; this asserts the transaction itself is not left open. */
    @Test
    public void s02031_cancelLeavesNoOpenTransaction() {
        enterTransform();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = viewportOf(workspace);
            final float[] start = handlePixel(NativeViewport.GIZMO_AXIS_X);
            final long when = SystemClock.uptimeMillis();
            sendFinger(viewport, when, when, MotionEvent.ACTION_DOWN, start[0], start[1]);
            sendFinger(viewport, when, when + 16L, MotionEvent.ACTION_MOVE,
                    start[0] + 20.0f, start[1]);
            sendFinger(viewport, when, when + 32L, MotionEvent.ACTION_CANCEL,
                    start[0] + 20.0f, start[1]);
            return null;
        });
        settle();
        assertEquals("nothing is still captured", 0.0,
                gizmoState()[NativeViewport.GIZMO_CAPTURING], EXACT);
        // An edit left open would make the next ordinary Apply record nothing.
        final int depthBefore = undoDepth();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyBoxTransform(4.0, 0.0, 0.0, 0.0, 0.0, 0.0);
            return null;
        });
        assertEquals("the next ordinary edit still records", depthBefore + 1, undoDepth());
    }

    /** S020-32. A second finger cancels the drag and restores the placement. */
    @Test
    public void s02032_aSecondPointerCancelsAndRestores() {
        enterTransform();
        final int depthBefore = undoDepth();
        final double[] before = placement();

        final double[] midDrag = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = viewportOf(workspace);
            final float[] start = handlePixel(NativeViewport.GIZMO_AXIS_X);
            final float[] step = screenStepAlong(NativeViewport.GIZMO_AXIS_X, 1.0f);
            final long when = SystemClock.uptimeMillis();

            final MotionEvent.PointerProperties[] props = {
                    pointerProperties(31, MotionEvent.TOOL_TYPE_FINGER),
                    pointerProperties(32, MotionEvent.TOOL_TYPE_FINGER)};
            final MotionEvent.PointerCoords[] one = {pointerCoords(start[0], start[1])};
            dispatch(viewport, when, when, MotionEvent.ACTION_DOWN, props, one, 1);
            for (int i = 1; i <= 6; i++) {
                final MotionEvent.PointerCoords[] moved = {
                        pointerCoords(start[0] + step[0] * i, start[1] + step[1] * i)};
                dispatch(viewport, when, when + 16L * i, MotionEvent.ACTION_MOVE, props, moved, 1);
            }
            final double[] moved = readPlacement();

            final MotionEvent.PointerCoords[] two = {
                    pointerCoords(start[0] + step[0] * 6, start[1] + step[1] * 6),
                    pointerCoords(start[0] + 200.0f, start[1] + 200.0f)};
            dispatch(viewport, when, when + 160L,
                    MotionEvent.ACTION_POINTER_DOWN
                            | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT),
                    props, two, 2);
            dispatch(viewport, when, when + 240L,
                    MotionEvent.ACTION_POINTER_UP
                            | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT),
                    props, two, 2);
            dispatch(viewport, when, when + 260L, MotionEvent.ACTION_UP, props, two, 1);
            return moved;
        });
        settle();

        assertTrue("precondition: the drag had actually moved the body",
                Math.abs(midDrag[0] - before[0]) > MOVED);
        assertArrayExactlyEquals("a second finger restores the pre-drag placement",
                before, placement());
        assertEquals("and leaves no step behind", depthBefore, undoDepth());
        assertEquals("and nothing captured", 0.0,
                gizmoState()[NativeViewport.GIZMO_CAPTURING], EXACT);
    }

    /** S020-33. A stylus drives the same solver, tracked by its own id. */
    @Test
    public void s02033_aStylusDragsAHandleThroughTheSameSolver() {
        enterTransform();
        final int depthBefore = undoDepth();
        final double[] before = placement();

        final double capturedPointer = onWorkspace(rule.getScenario(),
                (activity, workspace) -> {
            final View viewport = viewportOf(workspace);
            final float[] start = handlePixel(NativeViewport.GIZMO_AXIS_Z);
            final float[] step = screenStepAlong(NativeViewport.GIZMO_AXIS_Z, 1.0f);
            final long when = SystemClock.uptimeMillis();
            final MotionEvent.PointerProperties[] props = {
                    pointerProperties(77, MotionEvent.TOOL_TYPE_STYLUS)};
            dispatch(viewport, when, when, MotionEvent.ACTION_DOWN, props,
                    new MotionEvent.PointerCoords[] {pointerCoords(start[0], start[1])}, 1);
            final double[] held = new double[NativeViewport.GIZMO_STATE_SIZE];
            NativeViewport.gizmoState(held);
            final double captured = held[NativeViewport.GIZMO_POINTER_ID];
            for (int i = 1; i <= 10; i++) {
                dispatch(viewport, when, when + 16L * i, MotionEvent.ACTION_MOVE, props,
                        new MotionEvent.PointerCoords[] {
                                pointerCoords(start[0] + step[0] * i, start[1] + step[1] * i)}, 1);
            }
            dispatch(viewport, when, when + 240L, MotionEvent.ACTION_UP, props,
                    new MotionEvent.PointerCoords[] {
                            pointerCoords(start[0] + step[0] * 10, start[1] + step[1] * 10)}, 1);
            return captured;
        });
        settle();

        assertEquals("the stylus's own id was captured", 77.0, capturedPointer, EXACT);
        assertTrue("and it moved the body",
                Math.abs(placement()[2] - before[2]) > MOVED);
        assertEquals("as one step", depthBefore + 1, undoDepth());
    }

    /**
     * S020-34. Chrome standing over the viewport takes its own touches.
     *
     * <p>Structural rather than positional: the chrome sits above the viewport
     * in the workspace's stack, and Android never offers a consumed event to a
     * sibling underneath. A surface that consumes its own drag therefore cannot
     * reach a handle through it, whatever is drawn behind it.
     */
    @Test
    public void s02034_chromeOverTheViewportBlocksThroughHitsToTheGizmo() {
        enterTransform();
        final boolean consumed = onWorkspace(rule.getScenario(), (activity, workspace) ->
                WorkspaceTestSupport.dragConsumed(workspace.transformModeGroup()));
        assertTrue("the mode selector owns the touches that land on it", consumed);
        assertEquals("and nothing reached a handle", 0.0,
                gizmoState()[NativeViewport.GIZMO_CAPTURING], EXACT);
    }

    // -----------------------------------------------------------------------
    // S020-35..37 — reachable geometry
    // -----------------------------------------------------------------------

    /**
     * S020-35. The handle hit region meets the 48 dp floor.
     *
     * <p>Measured where it matters: a pixel offset perpendicular to the shaft by
     * just under 24 dp still hits it, and one well beyond does not. That is the
     * corridor as the finger experiences it, not as a constant claims it.
     */
    @Test
    public void s02035_handleHitRegionsMeetThe48DpFloor() {
        enterTransform();
        final boolean[] hits = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final float density = workspace.getResources().getDisplayMetrics().density;
            final float[] handle = handlePixel(NativeViewport.GIZMO_AXIS_X);
            final float[] along = screenStepAlong(NativeViewport.GIZMO_AXIS_X, 1.0f);
            final float length = (float) Math.hypot(along[0], along[1]);
            // Perpendicular to the shaft on screen, in pixels per dp.
            final float px = -along[1] / length * density;
            final float py = along[0] / length * density;
            final float inside = 22.0f;
            final float outside = 40.0f;
            return new boolean[] {
                    NativeViewport.gizmoHitTest(handle[0] + px * inside, handle[1] + py * inside)
                            == NativeViewport.GIZMO_AXIS_X,
                    NativeViewport.gizmoHitTest(handle[0] - px * inside, handle[1] - py * inside)
                            == NativeViewport.GIZMO_AXIS_X,
                    NativeViewport.gizmoHitTest(handle[0] + px * outside, handle[1] + py * outside)
                            == NativeViewport.GIZMO_AXIS_NONE};
        });
        assertTrue("22 dp to one side of the shaft still grabs it", hits[0]);
        assertTrue("and 22 dp to the other side", hits[1]);
        assertTrue("40 dp away does not", hits[2]);
    }

    /**
     * S020-36. The drawn gizmo stays within a readable band of screen sizes as
     * the camera dollies from close to far.
     */
    @Test
    public void s02036_gizmoScreenSizeStaysWithinItsBand() {
        enterTransform();
        final double[] lengths = new double[3];
        final float[] distances = {3.0f, 8.0f, 30.0f};
        for (int i = 0; i < distances.length; i++) {
            final int index = i;
            lengths[i] = onWorkspace(rule.getScenario(), (activity, workspace) -> {
                NativeViewport.debugSetCameraPose(0.9f, 0.6f, distances[index]);
                final float[] pivot = pivotPixelUnchecked();
                final float[] handle = handlePixel(NativeViewport.GIZMO_AXIS_Y);
                return Math.hypot(handle[0] - pivot[0], handle[1] - pivot[1]);
            });
        }
        final double density = onWorkspace(rule.getScenario(), (activity, workspace) ->
                (double) workspace.getResources().getDisplayMetrics().density);
        for (double lengthPixels : lengths) {
            final double dp = lengthPixels / density;
            assertTrue("the gizmo must never shrink below a fingertip (was " + dp + " dp)",
                    dp > 30.0);
            assertTrue("nor swallow the viewport (was " + dp + " dp)", dp < 160.0);
        }
    }

    /**
     * S020-37. The mode selector meets the 48 dp floor and collides with
     * nothing, in every window the workspace adapts to.
     */
    @Test
    public void s02037_theModeSelectorIsReachableAndCollisionFreeInEveryWindow() {
        final int[] orientations = {ActivityInfo.SCREEN_ORIENTATION_PORTRAIT,
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE};
        for (int orientation : orientations) {
            setOrientation(rule.getScenario(), orientation);
            settleLayout();
            enterTransform();
            settleLayout();

            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                final float density = workspace.getResources().getDisplayMetrics().density;
                final int floor = Math.round(TypedValue.applyDimension(
                        TypedValue.COMPLEX_UNIT_DIP, 48.0f,
                        workspace.getResources().getDisplayMetrics()));
                // The precision toggle is in this list on purpose. It is the
                // LAST child of the trailing cluster, so it is the one Android
                // squeezes when the column stops fitting — which is exactly what
                // adding a third capsule to a short window did before the pair
                // learned to turn on its side. A control that quietly drops to
                // 14 dp is the regression this case exists to catch.
                for (View control : new View[] {workspace.transformMoveAction(),
                        workspace.transformRotateAction(), workspace.precisionToggle()}) {
                    assertTrue("a mode control is on screen", control.getVisibility() == View.VISIBLE
                            && control.getWidth() > 0);
                    assertTrue("a mode control must meet the 48 dp floor: "
                                    + control.getWidth() + "x" + control.getHeight()
                                    + " at density " + density,
                            control.getWidth() >= floor && control.getHeight() >= floor);
                    assertTrue("and be fully on screen",
                            WorkspaceTestSupport.isFullyOnScreen(control, workspace));
                }

                final Rect selector = rectOf(workspace, workspace.transformModeGroup());
                for (View other : new View[] {workspace.precisionToggle(),
                        workspace.toolRailScroll(), workspace.objectsCapsule()}) {
                    if (other == null || other.getVisibility() != View.VISIBLE) {
                        continue;
                    }
                    final Rect otherRect = rectOf(workspace, other);
                    assertFalse("the selector must not cover " + other.getId()
                                    + " (" + selector + " vs " + otherRect + ")",
                            Rect.intersects(selector, otherRect));
                }
                return null;
            });
        }
    }

    // -----------------------------------------------------------------------
    // S020-38..41 — the transform costs the mesh nothing
    // -----------------------------------------------------------------------

    /** S020-38/39. A whole Move or Rotate drag, its commit, its undo and its
     *  redo publish no mesh revision at all. */
    @Test
    public void s02038and39_transformOnlyWorkPublishesNoMeshRevision() {
        enterTransform();
        for (final boolean rotate : new boolean[] {false, true}) {
            if (rotate) {
                selectRotate();
            }
            final long revisionBefore = NativeViewport.constructionMeshRevision();
            assertTrue(dragHandle(rotate ? NativeViewport.GIZMO_AXIS_Y
                    : NativeViewport.GIZMO_AXIS_X, 1.0f, 40));
            undo();
            redo();
            assertEquals((rotate ? "a rotate" : "a move")
                            + " drag, commit, undo and redo must publish no geometry",
                    revisionBefore, NativeViewport.constructionMeshRevision());
        }
    }

    /**
     * S020-40. The positive control: a shape change DOES publish, so the
     * counter above is not simply blind.
     */
    @Test
    public void s02040_aShapeChangeStillPublishes() {
        final long before = NativeViewport.constructionMeshRevision();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionSphere(1.75);
            return null;
        });
        assertNotEquals("a real geometry change must still mint a revision",
                before, NativeViewport.constructionMeshRevision());
    }

    /**
     * S020-41. The gizmo pass leaves the rest of the frame alone.
     *
     * <p>Asserted through what a renderer defect would actually break: the
     * viewport keeps presenting, and every display setting still behaves, with
     * handles on screen. A pixel comparison would be a screenshot test, which
     * this deliberately is not.
     */
    @Test
    public void s02041_theGizmoPassDoesNotDisturbTheRestOfTheFrame() {
        enterTransform();
        final long revisionBefore = NativeViewport.constructionMeshRevision();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.setGridVisible(false);
            NativeViewport.setGridVisible(true);
            NativeViewport.setViewportBackground(NativeViewport.VIEWPORT_BACKGROUND_LIGHT_CHARCOAL);
            NativeViewport.setViewportBackground(NativeViewport.VIEWPORT_BACKGROUND_WARM_GRAPHITE);
            return null;
        });
        settle();
        assertTrue("the gizmo is still on screen", gizmoVisible());
        assertEquals("and none of it touched geometry",
                revisionBefore, NativeViewport.constructionMeshRevision());
    }

    // -----------------------------------------------------------------------
    // Helpers
    //
    // Everything below asks native code where a handle is rather than knowing.
    // -----------------------------------------------------------------------

    private void enterTransform() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            selectConstructionTool(workspace, R.id.tool_rail_place);
            return null;
        });
        settleLayout();
    }

    private void selectRotate() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.transformRotateAction().performClick();
            return null;
        });
        settle();
        assertEquals("Rotate is held", NativeViewport.GIZMO_MODE_ROTATE, gizmoMode());
    }

    /**
     * Grabs one handle and drags it, returning whether the handle was there.
     *
     * <p>The start pixel and the drag direction both come from native code, so
     * the gesture follows the handle wherever the camera has put it. `scale` of
     * zero produces a tap.
     */
    private boolean dragHandle(final int axis, final float scale, final int steps) {
        final Boolean grabbed = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = viewportOf(workspace);
            final float[] start = new float[2];
            if (!NativeViewport.gizmoHandlePoint(axis, start)) {
                return Boolean.FALSE;
            }
            if (NativeViewport.gizmoHitTest(start[0], start[1]) != axis) {
                return Boolean.FALSE;
            }
            final float[] step = (gizmoMode() == NativeViewport.GIZMO_MODE_ROTATE)
                    ? screenTangentAt(axis, scale) : screenStepAlong(axis, scale);
            final long when = SystemClock.uptimeMillis();
            sendFinger(viewport, when, when, MotionEvent.ACTION_DOWN, start[0], start[1]);
            for (int i = 1; i <= steps; i++) {
                sendFinger(viewport, when, when + 16L * i, MotionEvent.ACTION_MOVE,
                        start[0] + step[0] * i, start[1] + step[1] * i);
            }
            sendFinger(viewport, when, when + 16L * (steps + 1), MotionEvent.ACTION_UP,
                    start[0] + step[0] * steps, start[1] + step[1] * steps);
            return Boolean.TRUE;
        });
        settle();
        return Boolean.TRUE.equals(grabbed);
    }

    /** One MOVE step, in pixels, along the axis as it is PROJECTED right now. */
    private static float[] screenStepAlong(int axis, float scale) {
        final float[] pivot = pivotPixelUnchecked();
        final float[] handle = handlePixel(axis);
        final float dx = handle[0] - pivot[0];
        final float dy = handle[1] - pivot[1];
        final float length = (float) Math.hypot(dx, dy);
        if (length < 1.0f) {
            // The axis is edge-on and has no screen direction. Any step is a
            // guess, so this returns none rather than inventing one.
            return new float[] {0.0f, 0.0f};
        }
        return new float[] {dx / length * 6.0f * scale, dy / length * 6.0f * scale};
    }

    /** One MOVE step, in pixels, TANGENT to the ring at the grab point. */
    private static float[] screenTangentAt(int axis, float scale) {
        final float[] along = screenStepAlong(axis, scale);
        return new float[] {-along[1], along[0]};
    }

    private static float[] handlePixel(int axis) {
        final float[] out = new float[2];
        assertTrue("native code must report where the " + axisName(axis) + " handle is",
                NativeViewport.gizmoHandlePoint(axis, out));
        return out;
    }

    private static float[] pivotPixelUnchecked() {
        final float[] out = new float[2];
        NativeViewport.gizmoHandlePoint(NativeViewport.GIZMO_AXIS_NONE, out);
        return out;
    }

    private float[] pivotPixel() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final float[] out = new float[2];
            assertTrue("the pivot must be on screen",
                    NativeViewport.gizmoHandlePoint(NativeViewport.GIZMO_AXIS_NONE, out));
            return out;
        });
    }

    private static String axisName(int axis) {
        switch (axis) {
            case NativeViewport.GIZMO_AXIS_X: return "X";
            case NativeViewport.GIZMO_AXIS_Y: return "Y";
            case NativeViewport.GIZMO_AXIS_Z: return "Z";
            default: return "pivot";
        }
    }

    private double[] placement() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> readPlacement());
    }

    /**
     * The authoritative placement: position X/Y/Z in meters then rotation
     * X/Y/Z in degrees, in that fixed order.
     *
     * <p>Read from native code every time and never remembered between calls.
     * A cached copy on this side would be exactly the second transform truth
     * the whole design exists to prevent.
     */
    private static double[] readPlacement() {
        final double[] values = new double[6];
        NativeViewport.boxTransform(values);
        return values;
    }

    private double[] placementOf(final long objectId) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final long previous = NativeViewport.sceneActiveBodyId();
            NativeViewport.sceneSelectBody(objectId);
            final double[] read = readPlacement();
            NativeViewport.sceneSelectBody(previous);
            return read;
        });
    }

    /**
     * What the exact-value editor has WRITTEN IN ITS FIELDS, parsed back.
     *
     * <p>Deliberately the field text and not a second native read: the point of
     * S020-25 is that the surface a user opens shows the gizmo's result, which a
     * native read would assert nothing at all about.
     */
    private static double[] readPlacementEditorValues(EditorWorkspaceView workspace) {
        final int[] ids = {R.id.field_pos_x, R.id.field_pos_y, R.id.field_pos_z,
                R.id.field_rot_x, R.id.field_rot_y, R.id.field_rot_z};
        final double[] shown = new double[ids.length];
        for (int i = 0; i < ids.length; i++) {
            final android.widget.EditText field = workspace.findViewById(ids[i]);
            assertTrue("the exact-value field must be on screen", field != null);
            shown[i] = Double.parseDouble(field.getText().toString().trim());
        }
        return shown;
    }

    private double[] gizmoState() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] out = new double[NativeViewport.GIZMO_STATE_SIZE];
            NativeViewport.gizmoState(out);
            return out;
        });
    }

    private boolean gizmoVisible() {
        return gizmoState()[NativeViewport.GIZMO_VISIBLE] != 0.0;
    }

    private int gizmoMode() {
        final double[] out = new double[NativeViewport.GIZMO_STATE_SIZE];
        NativeViewport.gizmoState(out);
        return (int) out[NativeViewport.GIZMO_MODE];
    }

    private int transformSelectorVisibility() {
        return onWorkspace(rule.getScenario(), (activity, workspace) ->
                workspace.transformModeGroup().getVisibility());
    }

    private int undoDepth() {
        return onWorkspace(rule.getScenario(), (activity, workspace) ->
                NativeViewport.constructionUndoDepth());
    }

    private void undo() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.undo_action).performClick();
            return null;
        });
        settle();
    }

    private void redo() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.findViewById(R.id.redo_action).performClick();
            return null;
        });
        settle();
    }

    private float[] cameraPose() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final float[] out = new float[NativeViewport.CAMERA_POSE_SIZE];
            NativeViewport.debugCameraPose(out);
            return out;
        });
    }

    private static Rect rectOf(View root, View child) {
        final Rect rect = new Rect(0, 0, child.getWidth(), child.getHeight());
        ((android.view.ViewGroup) root).offsetDescendantRectToMyCoords(child, rect);
        return rect;
    }

    private static boolean isFinite(double value) {
        return !Double.isNaN(value) && !Double.isInfinite(value);
    }

    private static void assertArrayExactlyEquals(String message, double[] expected,
                                                 double[] actual) {
        assertEquals(message + " (length)", expected.length, actual.length);
        for (int i = 0; i < expected.length; i++) {
            assertEquals(message + " (slot " + i + ")", expected[i], actual[i], EXACT);
        }
    }

    private static View viewportOf(EditorWorkspaceView workspace) {
        final View viewport = workspace.findViewById(R.id.viewport_surface);
        assertTrue("precondition: the viewport must be laid out",
                viewport != null && viewport.getWidth() > 0 && viewport.getHeight() > 0);
        return viewport;
    }

    private static boolean sendFinger(View viewport, long downTime, long eventTime, int action,
                                      float x, float y) {
        final MotionEvent event = MotionEvent.obtain(downTime, eventTime, action, x, y, 0);
        try {
            return viewport.dispatchTouchEvent(event);
        } finally {
            event.recycle();
        }
    }

    private static void dispatch(View viewport, long downTime, long eventTime, int action,
                                 MotionEvent.PointerProperties[] props,
                                 MotionEvent.PointerCoords[] coords, int count) {
        final MotionEvent event = MotionEvent.obtain(downTime, eventTime, action, count, props,
                coords, 0, 0, 1.0f, 1.0f, 0, 0, 0, 0);
        try {
            viewport.dispatchTouchEvent(event);
        } finally {
            event.recycle();
        }
    }

    private static MotionEvent.PointerProperties pointerProperties(int id, int toolType) {
        final MotionEvent.PointerProperties props = new MotionEvent.PointerProperties();
        props.id = id;
        props.toolType = toolType;
        return props;
    }

    private static MotionEvent.PointerCoords pointerCoords(float x, float y) {
        final MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
        coords.x = x;
        coords.y = y;
        coords.pressure = 1.0f;
        coords.size = 1.0f;
        return coords;
    }
}
