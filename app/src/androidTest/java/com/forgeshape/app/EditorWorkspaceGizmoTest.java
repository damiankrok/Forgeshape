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
 * Stage 020 and Stage 020R2: the Construction transform gizmo — Move, Rotate and
 * Scale, in World or Local axes — from the product side of JNI.
 *
 * <p>The solvers themselves — closest approach, the plane fallback, the signed
 * angle, the unwrap, accumulation past a full turn, the world/local composition,
 * the branch-continuous Euler decomposition, the screen-space scale mapping, the
 * screen-constant instrument size and the transaction boundary — are proved
 * against an isolated scene by the native {@code FORGESHAPE_GIZMO_SELFTEST} and
 * {@code FORGESHAPE_CONSTRUCTION_TRANSFORM_SELFTEST} suites, which is where they
 * belong: a domain rule that can only be shown through an Android view is a rule
 * nothing else can rely on. What this suite adds is everything that is genuinely about
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
            NativeViewport.applyBoxTransform(1.5, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
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
            NativeViewport.applyBoxTransform(3.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
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
        final int[] axes = {NativeViewport.GIZMO_HANDLE_AXIS_X, NativeViewport.GIZMO_HANDLE_AXIS_Y,
                NativeViewport.GIZMO_HANDLE_AXIS_Z};
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
        dragHandle(NativeViewport.GIZMO_HANDLE_AXIS_X, 1.0f, 16);
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
        final boolean grabbed = dragHandle(NativeViewport.GIZMO_HANDLE_AXIS_Y, 0.0f, 4);
        assertTrue("the handle was still grabbed", grabbed);
        assertEquals("a tap is not an edit", depthBefore, undoDepth());
        assertArrayExactlyEquals("and moved nothing", before, placement());
    }

    /** S020-12. A drag is one step no matter how many MOVE samples it carried. */
    @Test
    public void s02012_oneDragIsOneStepWhateverTheSampleCount() {
        enterTransform();
        final int depthBefore = undoDepth();
        final boolean dragged = dragHandle(NativeViewport.GIZMO_HANDLE_AXIS_X, 1.0f, 48);
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
            final float[] start = handlePixel(NativeViewport.GIZMO_HANDLE_AXIS_X);
            final float[] step = screenStepAlong(NativeViewport.GIZMO_HANDLE_AXIS_X, 1.0f);
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
        final int[] axes = {NativeViewport.GIZMO_HANDLE_AXIS_X, NativeViewport.GIZMO_HANDLE_AXIS_Y,
                NativeViewport.GIZMO_HANDLE_AXIS_Z};
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
            final float[] handle = handlePixel(NativeViewport.GIZMO_HANDLE_AXIS_Y);
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
        assertTrue("the ring was grabbed", dragHandle(NativeViewport.GIZMO_HANDLE_AXIS_Y, 1.0f, 24));
        assertEquals("one ring drag is one step", depthBefore + 1, undoDepth());

        final double[] before = placement();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = viewportOf(workspace);
            final float[] start = handlePixel(NativeViewport.GIZMO_HANDLE_AXIS_Y);
            final float[] step = screenTangentAt(NativeViewport.GIZMO_HANDLE_AXIS_Y, 1.0f);
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
        assertTrue(dragHandle(NativeViewport.GIZMO_HANDLE_AXIS_X, 1.0f, 10));
        final double[] afterMove = placement();

        selectRotate();
        assertTrue(dragHandle(NativeViewport.GIZMO_HANDLE_AXIS_Y, 1.0f, 10));
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
        assertTrue(dragHandle(NativeViewport.GIZMO_HANDLE_AXIS_X, 1.0f, 12));
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
            NativeViewport.applyBoxTransform(2.5, 1.25, -0.75, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
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
        assertTrue(dragHandle(NativeViewport.GIZMO_HANDLE_AXIS_X, 1.0f, 10));
        undo();
        assertTrue("there is something to redo", NativeViewport.constructionRedoAvailable());
        assertTrue(dragHandle(NativeViewport.GIZMO_HANDLE_AXIS_Y, 1.0f, 10));
        assertFalse("a new edit is a new branch", NativeViewport.constructionRedoAvailable());
    }

    /** S020-28. Two bodies undo in chronological order, each on its own. */
    @Test
    public void s02028_twoBodiesUndoChronologicallyAndIndependently() {
        enterTransform();
        final long first = NativeViewport.sceneActiveBodyId();
        assertTrue(dragHandle(NativeViewport.GIZMO_HANDLE_AXIS_X, 1.0f, 10));
        final double firstX = placement()[0];

        final long second = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final long created = NativeViewport.sceneAddBody();
            workspace.syncFromNative();
            return created;
        });
        settle();
        assertTrue(dragHandle(NativeViewport.GIZMO_HANDLE_AXIS_Y, 1.0f, 10));
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
        assertTrue(dragHandle(NativeViewport.GIZMO_HANDLE_AXIS_X, 1.0f, 12));
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
                    NativeViewport.GIZMO_HANDLE_NONE, NativeViewport.gizmoHitTest(x, y));
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
            final float[] start = handlePixel(NativeViewport.GIZMO_HANDLE_AXIS_X);
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
            NativeViewport.applyBoxTransform(4.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
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
            final float[] start = handlePixel(NativeViewport.GIZMO_HANDLE_AXIS_X);
            final float[] step = screenStepAlong(NativeViewport.GIZMO_HANDLE_AXIS_X, 1.0f);
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
            final float[] start = handlePixel(NativeViewport.GIZMO_HANDLE_AXIS_Z);
            final float[] step = screenStepAlong(NativeViewport.GIZMO_HANDLE_AXIS_Z, 1.0f);
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
            final float[] handle = handlePixel(NativeViewport.GIZMO_HANDLE_AXIS_X);
            final float[] along = screenStepAlong(NativeViewport.GIZMO_HANDLE_AXIS_X, 1.0f);
            final float length = (float) Math.hypot(along[0], along[1]);
            // Perpendicular to the shaft on screen, in pixels per dp.
            final float px = -along[1] / length * density;
            final float py = along[0] / length * density;
            final float inside = 22.0f;
            final float outside = 40.0f;
            return new boolean[] {
                    NativeViewport.gizmoHitTest(handle[0] + px * inside, handle[1] + py * inside)
                            == NativeViewport.GIZMO_HANDLE_AXIS_X,
                    NativeViewport.gizmoHitTest(handle[0] - px * inside, handle[1] - py * inside)
                            == NativeViewport.GIZMO_HANDLE_AXIS_X,
                    NativeViewport.gizmoHitTest(handle[0] + px * outside, handle[1] + py * outside)
                            == NativeViewport.GIZMO_HANDLE_NONE};
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
                final float[] handle = handlePixel(NativeViewport.GIZMO_HANDLE_AXIS_Y);
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
            assertTrue(dragHandle(rotate ? NativeViewport.GIZMO_HANDLE_AXIS_Y
                    : NativeViewport.GIZMO_HANDLE_AXIS_X, 1.0f, 40));
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
    // S020R2 — the full transform: planes, spaces and Scale
    //
    // The mathematics of all of it — world versus local composition, the
    // branch-continuous Euler decomposition, the plane and scale solvers, the
    // basis freeze — is proved against isolated scenes by the native
    // FORGESHAPE_GIZMO_SELFTEST and FORGESHAPE_CONSTRUCTION_TRANSFORM_SELFTEST
    // suites. What these cases add is the product: that the controls exist and
    // withdraw where they must, that a real MotionEvent through the real
    // SurfaceView reaches the new handles, that the exact-value surface and the
    // handles are the same truth, and that a scale still costs the mesh nothing.
    // -----------------------------------------------------------------------

    /**
     * S020R2-01. Scale is domain state: it defaults to one, survives an exact
     * Apply, and undoes and redoes with everything else.
     */
    @Test
    public void s020r201_scaleRoundTripsThroughHistory() {
        enterTransform();
        final double[] fresh = placement();
        assertEquals("a body is born at its true size", 1.0, fresh[SCALE_X], EXACT);
        assertEquals(1.0, fresh[SCALE_Y], EXACT);
        assertEquals(1.0, fresh[SCALE_Z], EXACT);

        applyPlacement(0.5, -0.25, 1.0, 10.0, -20.0, 30.0, 3.0, 0.5, 2.25);
        final double[] applied = placement();
        assertEquals("the scale landed", 3.0, applied[SCALE_X], EXACT);
        assertEquals(0.5, applied[SCALE_Y], EXACT);
        assertEquals(2.25, applied[SCALE_Z], EXACT);

        undo();
        assertArrayExactlyEquals("undo restores all nine values", fresh, placement());
        redo();
        assertArrayExactlyEquals("and redo puts all nine back", applied, placement());
    }

    /**
     * S020R2-02. One Apply is ONE atomic transaction over position, rotation
     * and scale.
     *
     * <p>A refused scale must leave the position and the rotation exactly as
     * they were and record nothing — a half-applied transform is the defect
     * that fail-closed validation exists to prevent — and an Apply that changes
     * nothing must record nothing either.
     */
    @Test
    public void s020r202_exactApplyIsAtomicAndRefusesAnImpossibleScale() {
        enterTransform();
        applyPlacement(1.0, 2.0, 3.0, 5.0, 10.0, 15.0, 2.0, 2.0, 2.0);
        final double[] before = placement();
        final int depthBefore = undoDepth();

        for (final double bad : new double[] {0.0, -1.0}) {
            final int status = onWorkspace(rule.getScenario(), (activity, workspace) -> {
                final int result = NativeViewport.applyBoxTransform(
                        9.0, 9.0, 9.0, 90.0, 90.0, 90.0, bad, 1.0, 1.0);
                workspace.syncFromNative();
                return result;
            });
            settle();
            assertEquals("a scale of " + bad + " is refused as not positive",
                    NativeViewport.APPLY_REJECTED_NOT_POSITIVE, status);
            assertArrayExactlyEquals("and nothing at all was written (scale " + bad + ")",
                    before, placement());
            assertEquals("a refused Apply records no step", depthBefore, undoDepth());
        }

        final int unchanged = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int result = NativeViewport.applyBoxTransform(before[0], before[1], before[2],
                    before[3], before[4], before[5], before[6], before[7], before[8]);
            workspace.syncFromNative();
            return result;
        });
        settle();
        assertEquals("re-applying the same nine values is Unchanged",
                NativeViewport.APPLY_UNCHANGED, unchanged);
        assertEquals("and a no-op records no step", depthBefore, undoDepth());
    }

    /**
     * S020R2-03 / S020R2-10. Every plane handle moves the body in its own plane
     * and never off it, through the real gesture path.
     *
     * <p>The third coordinate is asserted EXACTLY equal, not nearly: the solver
     * projects the pointer displacement onto two directions rather than
     * computing three and correcting one, so staying in the plane is a property
     * of the arithmetic and not a rounding accident.
     */
    @Test
    public void s020r203_everyMovePlaneMovesOnlyInItsPlane() {
        final int[] planes = {NativeViewport.GIZMO_HANDLE_PLANE_XY,
                NativeViewport.GIZMO_HANDLE_PLANE_XZ, NativeViewport.GIZMO_HANDLE_PLANE_YZ};
        final int[][] moved = {{0, 1}, {0, 2}, {1, 2}};
        final int[] held = {2, 1, 0};
        for (int i = 0; i < planes.length; i++) {
            resetToBaselineConstruction(rule.getScenario());
            enterTransform();
            requireHandleReachable(planes[i]);
            final double[] before = placement();
            assertTrue("the plane handle was grabbed", dragHandle(planes[i], 1.0f, 12));
            final double[] after = placement();

            assertTrue("a plane drag must move the body",
                    Math.abs(after[moved[i][0]] - before[moved[i][0]]) > MOVED
                            || Math.abs(after[moved[i][1]] - before[moved[i][1]]) > MOVED);
            assertEquals("and must never leave its plane",
                    before[held[i]], after[held[i]], EXACT);
            assertEquals("a move never turns the body", before[3], after[3], EXACT);
            assertEquals("nor resizes it", before[SCALE_X], after[SCALE_X], EXACT);
            assertEquals("one plane drag is one step", 1, undoDepth());
        }
    }

    /**
     * S020R2-04. Local space moves the body along its OWN axis.
     *
     * <p>The same handle name, the same gesture, a body turned so that its local
     * X is nowhere near the world X: in World only the X coordinate moves, and
     * in Local every world coordinate does. That difference is the whole reason
     * the space selector exists.
     */
    @Test
    public void s020r204_localSpaceMovesAlongTheBodyOwnAxis() {
        enterTransform();
        applyPlacement(0.0, 0.0, 0.0, 37.0, -52.0, 24.0, 1.0, 1.0, 1.0);

        requireHandleReachable(NativeViewport.GIZMO_HANDLE_AXIS_X);
        assertTrue("the world X handle was grabbed",
                dragHandle(NativeViewport.GIZMO_HANDLE_AXIS_X, 1.0f, 12));
        final double[] world = placement();
        assertTrue("a world X drag moves X", Math.abs(world[0]) > MOVED);
        assertEquals("and leaves Y exactly alone", 0.0, world[1], EXACT);
        assertEquals("and Z", 0.0, world[2], EXACT);

        undo();
        selectSpace(NativeViewport.GIZMO_SPACE_LOCAL);
        assertEquals("the session is in Local", NativeViewport.GIZMO_SPACE_LOCAL, gizmoSpace());
        requireHandleReachable(NativeViewport.GIZMO_HANDLE_AXIS_X);
        assertTrue("the local X handle was grabbed",
                dragHandle(NativeViewport.GIZMO_HANDLE_AXIS_X, 1.0f, 12));
        final double[] local = placement();
        assertTrue("a local X drag on a turned body moves Y as well",
                Math.abs(local[1]) > MOVED);
        assertTrue("and Z", Math.abs(local[2]) > MOVED);
        assertEquals("and it is still a move, not a turn", 37.0, local[3], EXACT);
    }

    /**
     * S020R2-05 / -06 / -07. World and Local rotation are different, correct
     * answers to the same gesture on the same mixed body.
     *
     * <p>Which one is right for which space is asserted against matrices by the
     * native suite. What matters here is that the space selector reaches the
     * solver at all: the same ring, dragged the same way, must not produce the
     * same orientation in the two spaces — and on a mixed body a correct
     * rotation legitimately moves more than one Euler field, which is why this
     * asserts nothing about which field moved.
     */
    @Test
    public void s020r205to07_worldAndLocalRotationDifferOnAMixedBody() {
        enterTransform();
        applyPlacement(0.0, 0.0, 0.0, 37.0, -52.0, 24.0, 1.0, 1.0, 1.0);
        final double[] start = placement();

        selectRotate();
        requireHandleReachable(NativeViewport.GIZMO_HANDLE_AXIS_X);
        assertTrue("the world X ring was grabbed",
                dragHandle(NativeViewport.GIZMO_HANDLE_AXIS_X, 1.0f, 14));
        final double[] worldTurned = placement();
        assertTrue("the world ring turned the body", changed(start, worldTurned, 3, 6));

        undo();
        selectSpace(NativeViewport.GIZMO_SPACE_LOCAL);
        requireHandleReachable(NativeViewport.GIZMO_HANDLE_AXIS_X);
        assertTrue("the local X ring was grabbed",
                dragHandle(NativeViewport.GIZMO_HANDLE_AXIS_X, 1.0f, 14));
        final double[] localTurned = placement();
        assertTrue("the local ring turned the body", changed(start, localTurned, 3, 6));

        assertTrue("world and local are different answers to the same gesture",
                changed(worldTurned, localTurned, 3, 6));
        assertEquals("and neither moved the body", 0.0, localTurned[0], EXACT);
    }

    /**
     * S020R2-09 / -10 / -11. The three kinds of scale handle: one axis, two
     * axes together, and all three at once with the ratios preserved.
     */
    @Test
    public void s020r209to11_axisPlaneAndUniformScale() {
        final int[] axes = {NativeViewport.GIZMO_HANDLE_AXIS_X, NativeViewport.GIZMO_HANDLE_AXIS_Y,
                NativeViewport.GIZMO_HANDLE_AXIS_Z};
        for (int i = 0; i < axes.length; i++) {
            resetToBaselineConstruction(rule.getScenario());
            enterTransform();
            selectScale();
            requireHandleReachable(axes[i]);
            assertTrue("the scale handle was grabbed", dragHandle(axes[i], 1.0f, 12));
            final double[] after = placement();
            assertNotEquals("the grabbed axis changed size", 1.0, after[SCALE_X + i]);
            for (int other = 0; other < 3; other++) {
                if (other != i) {
                    assertEquals("an axis scale touches no other axis",
                            1.0, after[SCALE_X + other], EXACT);
                }
            }
            assertEquals("a scale never moves the body", 0.0, after[0], EXACT);
            assertEquals("nor turns it", 0.0, after[3], EXACT);
            assertTrue("and a scale is always positive", after[SCALE_X + i] > 0.0);
        }

        final int[] planes = {NativeViewport.GIZMO_HANDLE_PLANE_XY,
                NativeViewport.GIZMO_HANDLE_PLANE_XZ, NativeViewport.GIZMO_HANDLE_PLANE_YZ};
        final int[][] pair = {{0, 1}, {0, 2}, {1, 2}};
        final int[] untouched = {2, 1, 0};
        for (int i = 0; i < planes.length; i++) {
            resetToBaselineConstruction(rule.getScenario());
            enterTransform();
            selectScale();
            requireHandleReachable(planes[i]);
            assertTrue("the plane scale handle was grabbed", dragHandle(planes[i], 1.0f, 12));
            final double[] after = placement();
            assertNotEquals("the plane changed size", 1.0, after[SCALE_X + pair[i][0]]);
            assertEquals("both of its axes by the SAME factor",
                    after[SCALE_X + pair[i][0]], after[SCALE_X + pair[i][1]], 1e-12);
            assertEquals("and the third is untouched",
                    1.0, after[SCALE_X + untouched[i]], EXACT);
        }

        // Uniform, on a body that is already non-uniform, so "preserves the
        // ratios" is a claim with something to preserve.
        resetToBaselineConstruction(rule.getScenario());
        enterTransform();
        applyPlacement(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 3.0, 1.5, 0.5);
        selectScale();
        assertTrue("the uniform handle was grabbed", dragUniformScale(12));
        final double[] uniform = placement();
        final double factor = uniform[SCALE_X] / 3.0;
        assertTrue("the uniform handle changed the size", Math.abs(factor - 1.0) > 1e-3);
        assertEquals("Y by the same factor", factor, uniform[SCALE_Y] / 1.5, 1e-9);
        assertEquals("Z by the same factor", factor, uniform[SCALE_Z] / 0.5, 1e-9);
        assertTrue("and every component stays positive",
                uniform[SCALE_X] > 0.0 && uniform[SCALE_Y] > 0.0 && uniform[SCALE_Z] > 0.0);
    }

    /**
     * S020R2-12 / S020R2-13. A scale drag of any length is one step, a tap is
     * none, and a cancelled drag restores all nine values exactly.
     */
    @Test
    public void s020r212and13_oneScaleDragIsOneStepAndCancelRestoresExactly() {
        enterTransform();
        applyPlacement(0.5, 0.0, 0.0, 12.0, 0.0, 0.0, 2.0, 1.0, 0.5);
        final int depthAfterSetup = undoDepth();
        final double[] before = placement();

        selectScale();
        requireHandleReachable(NativeViewport.GIZMO_HANDLE_UNIFORM);
        assertTrue("a tap on the uniform handle is not a drag", dragUniformScale(0));
        assertEquals("a tap records nothing", depthAfterSetup, undoDepth());
        assertArrayExactlyEquals("and changes nothing", before, placement());

        assertTrue(dragUniformScale(48));
        assertEquals("a forty-eight sample drag is one step", depthAfterSetup + 1, undoDepth());
        final double[] scaled = placement();
        assertNotEquals(before[SCALE_X], scaled[SCALE_X]);

        undo();
        assertArrayExactlyEquals("undo restores all nine", before, placement());
        redo();
        assertArrayExactlyEquals("redo restores all nine", scaled, placement());

        // A second pointer arriving mid-drag cancels: the placement goes back
        // exactly and nothing is recorded.
        final int depthBeforeCancel = undoDepth();
        final double[] beforeCancel = placement();
        assertTrue("the cancelled drag was really started", startAndInterruptUniformDrag());
        assertArrayExactlyEquals("a cancelled scale drag restores all nine",
                beforeCancel, placement());
        assertEquals("and records nothing", depthBeforeCancel, undoDepth());
    }

    /**
     * S020R2-14. The handles and the exact-value surface are one truth, both
     * ways, including the scale.
     */
    @Test
    public void s020r214_gizmoAndExactTransformAreOneTruth() {
        enterTransform();
        selectScale();
        requireHandleReachable(NativeViewport.GIZMO_HANDLE_UNIFORM);
        assertTrue(dragUniformScale(14));
        final double[] fromDrag = placement();

        final double[] shown = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            openPrecision(workspace);
            return readPlacementEditorValues(workspace);
        });
        settleLayout();
        for (int i = 0; i < 3; i++) {
            assertEquals("the exact Scale " + i + " is the handle result",
                    fromDrag[SCALE_X + i], shown[SCALE_X + i], 1e-4);
        }
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            closePrecision(workspace);
            return null;
        });
        settleLayout();

        // And the other direction: a typed scale is what the handles then act
        // on, so the next drag starts from the typed value rather than from a
        // remembered one.
        applyPlacement(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 4.0, 4.0, 4.0);
        assertTrue(dragUniformScale(10));
        assertNotEquals("the drag moved the typed scale", 4.0, placement()[SCALE_X]);
        assertTrue("and started from it rather than from one",
                placement()[SCALE_X] > 2.0);
    }

    /**
     * S020R2-15. A non-uniform scale reaches picking and does NOT reach the
     * instrument.
     *
     * <p>Two separate claims, both of which a wrong matrix would break. The
     * gizmo is sized from the camera alone, so stretching a body must leave its
     * handles exactly where they were; and picking consumes the same transform
     * the renderer does, so a pixel that missed the body before the stretch must
     * hit it afterwards.
     */
    @Test
    public void s020r215_nonUniformScaleReachesPickingAndNotTheGizmo() {
        enterTransform();
        final long body = NativeViewport.sceneActiveBodyId();

        // Pulled back far enough that a body six meters below the origin is
        // comfortably on screen, and held there for the whole case so the two
        // instrument measurements below are taken from one viewpoint.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.debugSetCameraPose(0.7f, 0.5f, 20.0f);
            return null;
        });
        settle();

        // Parked well clear of the origin, and of the +X and +Y neighbourhoods
        // other cases leave bodies in. The scene is process-scoped and has no
        // delete, so a case that probed a pixel near the origin would be
        // asserting something about whatever the previous case left behind.
        applyPlacement(0.0, -6.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
        final double[] handleBefore = handleOffsetFromPivot();

        // Three meters out along world X from THIS body: outside the baseline
        // box, which is two meters wide and therefore reaches one meter.
        final float[] probe = pixelOfWorldPoint(3.0, -6.0, 0.0);

        // Something else has to be active for a MISS to be observable: a tap
        // that hits nothing deliberately leaves the edit target alone, so a
        // miss while this body was already active would prove nothing.
        final long parked = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final long created = NativeViewport.sceneAddBody();
            NativeViewport.applyBoxTransform(0.0, 9.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
            workspace.syncFromNative();
            return created;
        });
        settle();
        assertNotEquals("precondition: a second body exists", body, parked);
        assertEquals("precondition: it is the active one",
                parked, NativeViewport.sceneActiveBodyId());

        tapViewport(probe[0], probe[1]);
        assertNotEquals("three meters out misses an unstretched body (probe " + probe[0] + ","
                        + probe[1] + ")",
                body, NativeViewport.sceneActiveBodyId());

        // Stretch it six times along X and try the same pixel again.
        applySelectedPlacement(body, 0.0, -6.0, 0.0, 6.0, 1.0, 1.0);
        tapViewport(probe[0], probe[1]);
        assertEquals("and hits it once it reaches six meters each way",
                body, NativeViewport.sceneActiveBodyId());

        // The instrument is unmoved by all of it.
        final double[] handleAfter = handleOffsetFromPivot();
        assertEquals("a stretched body does not stretch its own gizmo",
                handleBefore[0], handleAfter[0], 1.0);
        assertEquals(handleBefore[1], handleAfter[1], 1.0);
    }

    /**
     * S020R2-16. Every handle a mode offers is classified correctly at its own
     * pixel, and the captured one is reported while it is held.
     */
    @Test
    public void s020r216_everyHandleIsClassifiedAtItsOwnPixel() {
        enterTransform();
        final int[][] perMode = {
                {NativeViewport.GIZMO_MODE_MOVE, NativeViewport.GIZMO_HANDLE_AXIS_X,
                        NativeViewport.GIZMO_HANDLE_AXIS_Y, NativeViewport.GIZMO_HANDLE_AXIS_Z,
                        NativeViewport.GIZMO_HANDLE_PLANE_XY,
                        NativeViewport.GIZMO_HANDLE_PLANE_XZ,
                        NativeViewport.GIZMO_HANDLE_PLANE_YZ},
                {NativeViewport.GIZMO_MODE_ROTATE, NativeViewport.GIZMO_HANDLE_AXIS_X,
                        NativeViewport.GIZMO_HANDLE_AXIS_Y, NativeViewport.GIZMO_HANDLE_AXIS_Z},
                {NativeViewport.GIZMO_MODE_SCALE, NativeViewport.GIZMO_HANDLE_AXIS_X,
                        NativeViewport.GIZMO_HANDLE_AXIS_Y, NativeViewport.GIZMO_HANDLE_AXIS_Z,
                        NativeViewport.GIZMO_HANDLE_PLANE_XY,
                        NativeViewport.GIZMO_HANDLE_PLANE_XZ,
                        NativeViewport.GIZMO_HANDLE_PLANE_YZ,
                        NativeViewport.GIZMO_HANDLE_UNIFORM},
        };
        for (final int[] mode : perMode) {
            selectMode(mode[0]);
            for (int i = 1; i < mode.length; i++) {
                final int handle = mode[i];
                // Orbited to first, exactly as a user would: an edge-on handle
                // is deliberately not grabbable, so the claim being made is
                // "from a viewpoint where this handle can be seen, its own pixel
                // names it" — not "every handle is grabbable from everywhere".
                assertTrue("no viewpoint reaches mode " + mode[0] + " handle " + handle,
                        orientCameraForHandle(handle));
                final int classified = onWorkspace(rule.getScenario(), (activity, workspace) -> {
                    final float[] point = new float[2];
                    if (!NativeViewport.gizmoHandlePoint(handle, point)) {
                        return -1;
                    }
                    return NativeViewport.gizmoHitTest(point[0], point[1]);
                });
                assertEquals("mode " + mode[0] + " handle " + handle
                        + " must be what its own pixel names", handle, classified);
            }
        }

        // The pivot is the uniform handle in Scale and NO handle in Move, which
        // is the tier rule stated where a user can feel it.
        selectMode(NativeViewport.GIZMO_MODE_SCALE);
        assertEquals("the pivot is the uniform scale handle",
                NativeViewport.GIZMO_HANDLE_UNIFORM, hitTestAtPivot());
        selectMode(NativeViewport.GIZMO_MODE_MOVE);
        assertEquals("and names nothing in Move, where every shaft meets there",
                NativeViewport.GIZMO_HANDLE_NONE, hitTestAtPivot());

        // While a handle is held, the session reports which one.
        selectMode(NativeViewport.GIZMO_MODE_SCALE);
        final double[] held = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = viewportOf(workspace);
            final float[] point = new float[2];
            NativeViewport.gizmoHandlePoint(NativeViewport.GIZMO_HANDLE_PLANE_XZ, point);
            final long when = SystemClock.uptimeMillis();
            sendFinger(viewport, when, when, MotionEvent.ACTION_DOWN, point[0], point[1]);
            sendFinger(viewport, when, when + 16L, MotionEvent.ACTION_MOVE, point[0] + 20.0f,
                    point[1] + 20.0f);
            final double[] state = new double[NativeViewport.GIZMO_STATE_SIZE];
            NativeViewport.gizmoState(state);
            sendFinger(viewport, when, when + 32L, MotionEvent.ACTION_CANCEL, point[0] + 20.0f,
                    point[1] + 20.0f);
            return state;
        });
        settle();
        assertEquals("the held handle is reported while it is held",
                NativeViewport.GIZMO_HANDLE_PLANE_XZ, (int) held[NativeViewport.GIZMO_HANDLE]);
        assertTrue("and the session says it is capturing",
                held[NativeViewport.GIZMO_CAPTURING] != 0.0);
    }

    /**
     * S020R2-18. Scale is Local-only, and the space selector is <b>absent</b>
     * there rather than shown and refused.
     *
     * <p>Leaving Scale puts the remembered space back, so a round trip through
     * Scale does not quietly change what a Move handle means. None of it is an
     * edit.
     */
    @Test
    public void s020r218_scaleIsLocalOnlyAndRestoresTheRememberedSpace() {
        enterTransform();
        final int depthBefore = undoDepth();
        final long revisionBefore = NativeViewport.constructionMeshRevision();

        assertEquals("Transform opens in World", NativeViewport.GIZMO_SPACE_WORLD, gizmoSpace());
        assertEquals("and the choice is offered", View.VISIBLE, spaceSelectorVisibility());

        selectSpace(NativeViewport.GIZMO_SPACE_LOCAL);
        selectScale();
        assertEquals("Scale is Local", NativeViewport.GIZMO_SPACE_LOCAL, gizmoSpace());
        assertEquals("and withdraws the choice", View.GONE, spaceSelectorVisibility());

        final boolean refused = onWorkspace(rule.getScenario(), (activity, workspace) ->
                !NativeViewport.setGizmoSpace(NativeViewport.GIZMO_SPACE_WORLD));
        assertTrue("and the guard below JNI refuses World anyway", refused);
        assertEquals("still Local", NativeViewport.GIZMO_SPACE_LOCAL, gizmoSpace());

        selectMode(NativeViewport.GIZMO_MODE_MOVE);
        assertEquals("leaving Scale restores the remembered space",
                NativeViewport.GIZMO_SPACE_LOCAL, gizmoSpace());
        assertEquals("and offers the choice again", View.VISIBLE, spaceSelectorVisibility());

        selectSpace(NativeViewport.GIZMO_SPACE_WORLD);
        selectScale();
        selectMode(NativeViewport.GIZMO_MODE_ROTATE);
        assertEquals("World is remembered just as well",
                NativeViewport.GIZMO_SPACE_WORLD, gizmoSpace());

        assertEquals("none of it is an edit", depthBefore, undoDepth());
        assertEquals("and none of it publishes geometry",
                revisionBefore, NativeViewport.constructionMeshRevision());
    }

    /**
     * S020R2-18 (layout). Both selectors meet the 48 dp floor and collide with
     * nothing, in every window the workspace adapts to.
     *
     * <p>The precision toggle is measured with them on purpose: it is the LAST
     * child of the trailing cluster, so it is the one Android squeezes when the
     * column stops fitting — which is exactly what adding capsules to a short
     * window does if the selectors do not turn on their side.
     */
    @Test
    public void s020r218_bothSelectorsAreReachableAndCollisionFreeInEveryWindow() {
        final int[] orientations = {ActivityInfo.SCREEN_ORIENTATION_PORTRAIT,
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE};
        for (int orientation : orientations) {
            setOrientation(rule.getScenario(), orientation);
            settleLayout();
            enterTransform();
            settleLayout();

            doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                final int floor = Math.round(TypedValue.applyDimension(
                        TypedValue.COMPLEX_UNIT_DIP, 48.0f,
                        workspace.getResources().getDisplayMetrics()));
                for (View control : new View[] {workspace.transformMoveAction(),
                        workspace.transformRotateAction(), workspace.transformScaleAction(),
                        workspace.transformSpaceWorldAction(),
                        workspace.transformSpaceLocalAction(), workspace.precisionToggle()}) {
                    assertTrue("a transform control is on screen",
                            control.getVisibility() == View.VISIBLE && control.getWidth() > 0);
                    assertTrue("it must meet the 48 dp floor: " + control.getWidth() + "x"
                                    + control.getHeight(),
                            control.getWidth() >= floor && control.getHeight() >= floor);
                    assertTrue("and be fully on screen",
                            WorkspaceTestSupport.isFullyOnScreen(control, workspace));
                }

                final Rect mode = rectOf(workspace, workspace.transformModeGroup());
                final Rect space = rectOf(workspace, workspace.transformSpaceGroup());
                final String cluster = "rail=" + rectOf(workspace, workspace.toolRailScroll())
                        + " mode=" + mode + " space=" + space
                        + " precision=" + rectOf(workspace, workspace.precisionGroup())
                        + " column=" + rectOf(workspace, workspace.railColumn());
                assertFalse("the two selectors must not cover each other " + cluster,
                        Rect.intersects(mode, space));
                for (View other : new View[] {workspace.precisionToggle(),
                        workspace.toolRailScroll(), workspace.objectsCapsule()}) {
                    if (other == null || other.getVisibility() != View.VISIBLE) {
                        continue;
                    }
                    final Rect otherRect = rectOf(workspace, other);
                    assertFalse("the mode selector must not cover " + other.getId() + " "
                                    + otherRect + " :: " + cluster,
                            Rect.intersects(mode, otherRect));
                    assertFalse("the space selector must not cover " + other.getId() + " "
                                    + otherRect + " :: " + cluster,
                            Rect.intersects(space, otherRect));
                }
                return null;
            });
        }
    }

    /**
     * S020R2-20. A long scale drag, and undoing and redoing it, costs the mesh
     * nothing — and a shape change still costs it something, so the counter is
     * proved to be able to move at all.
     */
    @Test
    public void s020r220_scaleAndItsHistoryPublishNoMeshRevision() {
        enterTransform();
        final long before = NativeViewport.constructionMeshRevision();

        selectScale();
        assertTrue(dragUniformScale(60));
        requireHandleReachable(NativeViewport.GIZMO_HANDLE_AXIS_Y);
        assertTrue(dragHandle(NativeViewport.GIZMO_HANDLE_AXIS_Y, 1.0f, 40));
        requireHandleReachable(NativeViewport.GIZMO_HANDLE_PLANE_XZ);
        assertTrue(dragHandle(NativeViewport.GIZMO_HANDLE_PLANE_XZ, 1.0f, 40));
        undo();
        undo();
        redo();
        assertEquals("no amount of scaling, undoing or redoing publishes geometry",
                before, NativeViewport.constructionMeshRevision());

        final long after = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyConstructionBox(3.0, 1.0, 0.5);
            workspace.syncFromNative();
            return NativeViewport.constructionMeshRevision();
        });
        settle();
        assertTrue("a real shape change still publishes: " + before + " -> " + after,
                after > before);
    }

    // -----------------------------------------------------------------------
    // Helpers
    //
    // Everything below asks native code where a handle is rather than knowing.
    // -----------------------------------------------------------------------

    /** Where the three scale values start in a placement array. */
    private static final int SCALE_X = NativeViewport.TRANSFORM_SCALE;
    private static final int SCALE_Y = NativeViewport.TRANSFORM_SCALE + 1;
    private static final int SCALE_Z = NativeViewport.TRANSFORM_SCALE + 2;

    private void enterTransform() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            selectConstructionTool(workspace, R.id.tool_rail_place);
            return null;
        });
        settleLayout();
    }

    private void selectRotate() {
        selectMode(NativeViewport.GIZMO_MODE_ROTATE);
    }

    private void selectScale() {
        selectMode(NativeViewport.GIZMO_MODE_SCALE);
    }

    /** Presses the mode button a user would press, then reads the mode back. */
    private void selectMode(final int mode) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            switch (mode) {
                case NativeViewport.GIZMO_MODE_ROTATE:
                    workspace.transformRotateAction().performClick();
                    break;
                case NativeViewport.GIZMO_MODE_SCALE:
                    workspace.transformScaleAction().performClick();
                    break;
                default:
                    workspace.transformMoveAction().performClick();
                    break;
            }
            return null;
        });
        settle();
        assertEquals("the requested mode is held", mode, gizmoMode());
    }

    /** Presses the space button a user would press, then reads it back. */
    private void selectSpace(final int space) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            if (space == NativeViewport.GIZMO_SPACE_LOCAL) {
                workspace.transformSpaceLocalAction().performClick();
            } else {
                workspace.transformSpaceWorldAction().performClick();
            }
            return null;
        });
        settle();
        assertEquals("the requested space is held", space, gizmoSpace());
    }

    /** One atomic exact-value Apply of all nine values, through the one entry point. */
    private void applyPlacement(final double px, final double py, final double pz,
                                final double rx, final double ry, final double rz,
                                final double sx, final double sy, final double sz) {
        final int status = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int result =
                    NativeViewport.applyBoxTransform(px, py, pz, rx, ry, rz, sx, sy, sz);
            workspace.syncFromNative();
            return result;
        });
        settle();
        assertTrue("the placement must land: status " + status,
                status == NativeViewport.APPLY_APPLIED
                        || status == NativeViewport.APPLY_UNCHANGED);
    }

    /**
     * Drags the UNIFORM scale handle, which sits on the pivot and therefore has
     * no projected direction of its own.
     *
     * <p>The gesture is the screen diagonal — right and up — which is what the
     * solver reads for this handle and the only direction it has. `steps` of
     * zero produces a tap.
     */
    private boolean dragUniformScale(final int steps) {
        final Boolean grabbed = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = viewportOf(workspace);
            final float[] start = new float[2];
            if (!NativeViewport.gizmoHandlePoint(NativeViewport.GIZMO_HANDLE_UNIFORM, start)) {
                return Boolean.FALSE;
            }
            if (NativeViewport.gizmoHitTest(start[0], start[1])
                    != NativeViewport.GIZMO_HANDLE_UNIFORM) {
                return Boolean.FALSE;
            }
            final long when = SystemClock.uptimeMillis();
            sendFinger(viewport, when, when, MotionEvent.ACTION_DOWN, start[0], start[1]);
            for (int i = 1; i <= steps; i++) {
                sendFinger(viewport, when, when + 16L * i, MotionEvent.ACTION_MOVE,
                        start[0] + 3.0f * i, start[1] - 3.0f * i);
            }
            sendFinger(viewport, when, when + 16L * (steps + 1), MotionEvent.ACTION_UP,
                    start[0] + 3.0f * steps, start[1] - 3.0f * steps);
            return Boolean.TRUE;
        });
        settle();
        return Boolean.TRUE.equals(grabbed);
    }

    /**
     * Starts a uniform scale drag, moves it, then puts a SECOND finger down —
     * which is the documented cancel — and lifts both.
     */
    private boolean startAndInterruptUniformDrag() {
        final Boolean started = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = viewportOf(workspace);
            final float[] start = new float[2];
            if (!NativeViewport.gizmoHandlePoint(NativeViewport.GIZMO_HANDLE_UNIFORM, start)) {
                return Boolean.FALSE;
            }
            final long when = SystemClock.uptimeMillis();
            sendFinger(viewport, when, when, MotionEvent.ACTION_DOWN, start[0], start[1]);
            for (int i = 1; i <= 6; i++) {
                sendFinger(viewport, when, when + 16L * i, MotionEvent.ACTION_MOVE,
                        start[0] + 6.0f * i, start[1] - 6.0f * i);
            }
            final MotionEvent.PointerProperties[] props = {
                    pointerProperties(0, MotionEvent.TOOL_TYPE_FINGER),
                    pointerProperties(1, MotionEvent.TOOL_TYPE_FINGER)};
            final MotionEvent.PointerCoords[] coords = {
                    pointerCoords(start[0] + 36.0f, start[1] - 36.0f),
                    pointerCoords(start[0] + 160.0f, start[1] + 160.0f)};
            dispatch(viewport, when, when + 128L,
                    MotionEvent.ACTION_POINTER_DOWN
                            | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT),
                    props, coords, 2);
            dispatch(viewport, when, when + 144L,
                    MotionEvent.ACTION_POINTER_UP
                            | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT),
                    props, coords, 2);
            sendFinger(viewport, when, when + 160L, MotionEvent.ACTION_UP,
                    start[0] + 36.0f, start[1] - 36.0f);
            return Boolean.TRUE;
        });
        settle();
        return Boolean.TRUE.equals(started);
    }

    /**
     * The pixel a given WORLD point lands on, measured rather than computed.
     *
     * <p>There is no general project-a-point call across JNI, and inventing one
     * out of the gizmo scale would mean this suite carrying its own copy of the
     * projection — a second answer to the one question the whole design keeps in
     * one place. So the point is measured instead: the active body is placed
     * exactly there for a moment, its PIVOT pixel is read from the same
     * projection the hit test uses, and the body is put back. Nothing is
     * written down and nothing is re-derived.
     */
    private float[] pixelOfWorldPoint(final double x, final double y, final double z) {
        final double[] restore = placement();
        final float[] pixel = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyBoxTransform(x, y, z, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
            workspace.syncFromNative();
            final float[] out = new float[2];
            assertTrue("the probe point must be on screen",
                    NativeViewport.gizmoHandlePoint(NativeViewport.GIZMO_HANDLE_NONE, out));
            return out;
        });
        settle();
        applyPlacement(restore[0], restore[1], restore[2], restore[3], restore[4], restore[5],
                restore[6], restore[7], restore[8]);
        return pixel;
    }

    /**
     * Orbits to a viewpoint from which `handle` is actually reachable, and says
     * whether one was found.
     *
     * <p>Not every handle is grabbable from every angle, and that is correct
     * rather than a defect: a shaft pointing at the camera projects to nothing,
     * and a plane seen edge-on collapses onto the pivot, where the gizmo
     * deliberately names no handle at all. A user orbits until they can see the
     * handle they want; a case that refused to do the same would either be
     * testing one lucky viewpoint or asserting that an edge-on handle is
     * grabbable, which it must not be.
     *
     * <p>The viewpoint is accepted only when the hit test agrees that the
     * handle own pixel names that handle — the same agreement a finger gets.
     */
    private boolean orientCameraForHandle(final int handle) {
        final float[][] poses = {
                {0.7f, 0.5f}, {0.7f, -0.5f}, {2.3f, 0.5f}, {2.3f, -0.5f},
                {3.9f, 0.5f}, {3.9f, -0.5f}, {5.5f, 0.5f}, {5.5f, -0.5f},
                {1.5f, 1.0f}, {4.7f, 1.0f}, {1.5f, -1.0f}, {4.7f, -1.0f},
                {0.2f, 0.9f}, {2.9f, 0.2f}, {5.0f, -0.2f}, {3.2f, 1.2f},
        };
        for (final float[] pose : poses) {
            final boolean reachable = onWorkspace(rule.getScenario(), (activity, workspace) -> {
                NativeViewport.debugSetCameraPose(pose[0], pose[1],
                        WorkspaceTestSupport.BASELINE_CAMERA_DISTANCE);
                final float[] point = new float[2];
                if (!NativeViewport.gizmoHandlePoint(handle, point)) {
                    return Boolean.FALSE;
                }
                return NativeViewport.gizmoHitTest(point[0], point[1]) == handle;
            });
            if (reachable) {
                settle();
                return true;
            }
        }
        return false;
    }

    /** Orients for a handle and fails the case when no viewpoint reaches it. */
    private void requireHandleReachable(final int handle) {
        assertTrue("no viewpoint makes handle " + handle + " reachable",
                orientCameraForHandle(handle));
    }

    /** The X handle offset from the pivot, in pixels: how large the instrument
     *  is drawn right now, which the body own scale must not change. */
    private double[] handleOffsetFromPivot() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final float[] pivot = pivotPixelUnchecked();
            final float[] handle = handlePixel(NativeViewport.GIZMO_HANDLE_AXIS_X);
            return new double[] {handle[0] - pivot[0], handle[1] - pivot[1]};
        });
    }

    /** Places and sizes ONE named body, leaving whatever was active active. */
    private void applySelectedPlacement(final long objectId, final double px, final double py,
                                        final double pz, final double sx, final double sy,
                                        final double sz) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final long previous = NativeViewport.sceneActiveBodyId();
            NativeViewport.sceneSelectBody(objectId);
            NativeViewport.applyBoxTransform(px, py, pz, 0.0, 0.0, 0.0, sx, sy, sz);
            NativeViewport.sceneSelectBody(previous);
            workspace.syncFromNative();
            return null;
        });
        settle();
    }

    /** A tap, through the real viewport, at a pixel. */
    private void tapViewport(final float x, final float y) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = viewportOf(workspace);
            final long when = SystemClock.uptimeMillis();
            sendFinger(viewport, when, when, MotionEvent.ACTION_DOWN, x, y);
            sendFinger(viewport, when, when + 24L, MotionEvent.ACTION_UP, x, y);
            return null;
        });
        settle();
    }

    private int hitTestAtPivot() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final float[] pivot = new float[2];
            assertTrue("the pivot must be on screen",
                    NativeViewport.gizmoHandlePoint(NativeViewport.GIZMO_HANDLE_NONE, pivot));
            return NativeViewport.gizmoHitTest(pivot[0], pivot[1]);
        });
    }

    /** Whether any slot in [from, to) differs by more than a tap. */
    private static boolean changed(double[] a, double[] b, int from, int to) {
        for (int i = from; i < to; i++) {
            if (Math.abs(a[i] - b[i]) > MOVED) {
                return true;
            }
        }
        return false;
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
        NativeViewport.gizmoHandlePoint(NativeViewport.GIZMO_HANDLE_NONE, out);
        return out;
    }

    private float[] pivotPixel() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final float[] out = new float[2];
            assertTrue("the pivot must be on screen",
                    NativeViewport.gizmoHandlePoint(NativeViewport.GIZMO_HANDLE_NONE, out));
            return out;
        });
    }

    private static String axisName(int axis) {
        switch (axis) {
            case NativeViewport.GIZMO_HANDLE_AXIS_X: return "X";
            case NativeViewport.GIZMO_HANDLE_AXIS_Y: return "Y";
            case NativeViewport.GIZMO_HANDLE_AXIS_Z: return "Z";
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
        final double[] values = new double[NativeViewport.TRANSFORM_SIZE];
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
                R.id.field_rot_x, R.id.field_rot_y, R.id.field_rot_z,
                R.id.field_scale_x, R.id.field_scale_y, R.id.field_scale_z};
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

    private int gizmoSpace() {
        return (int) gizmoState()[NativeViewport.GIZMO_SPACE];
    }

    private int transformSelectorVisibility() {
        return onWorkspace(rule.getScenario(), (activity, workspace) ->
                workspace.transformModeGroup().getVisibility());
    }

    private int spaceSelectorVisibility() {
        return onWorkspace(rule.getScenario(), (activity, workspace) ->
                workspace.transformSpaceGroup().getVisibility());
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
