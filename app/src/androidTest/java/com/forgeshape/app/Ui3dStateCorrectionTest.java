package com.forgeshape.app;

import static com.forgeshape.app.SketchTestSupport.dragSketch;
import static com.forgeshape.app.SketchTestSupport.selectTool;
import static com.forgeshape.app.SketchTestSupport.sketchState;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.os.SystemClock;
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
 * `UI-3D-STATE-C1`: the correction gate for the audit's anchored-chrome family.
 *
 * <p>Where {@link Ui3dStateAuditTest} RECORDS, this ASSERTS. It is the red-green
 * gate for six findings of `UI-3D-STATE-AUDIT-R1`, all of them presentation and
 * all of them in one of two mechanisms:
 *
 * <ul>
 *   <li><b>The coordinate space</b> — {@code UI3D-F-001} placed every anchored
 *       surface one window inset below its anchor because a viewport-pixel
 *       anchor was written as a translation inside the inset-padded overlay, and
 *       {@code UI3D-F-006} clamped the CAD cluster into that padded box rather
 *       than into the viewport.</li>
 *   <li><b>The refresh</b> — {@code UI3D-F-002} (camera motion left the numbers
 *       behind), {@code UI3D-F-003} (absent on the frame the mode opened),
 *       {@code UI3D-F-004} (ghost UI into Sculpt and over a hidden body) and
 *       {@code UI3D-F-007} (the PREVIOUS body's anchors after a switch).</li>
 * </ul>
 *
 * <p><b>The tolerance is the audit's own</b> — {@link #TOLERANCE_DP} dp, the
 * number `UI3DC1-03` states — and the expected anchor is always re-read from
 * native AFTER the action, never remembered from before it, because a row that
 * matches the anchor from before and misses the one from after is exactly what a
 * stale screen coordinate looks like.
 *
 * <p><b>{@code UI3D-F-005} is deliberately not tested here.</b> That the
 * renderer has no case for {@code SketchOverlayStyle::Dimension} is a separate
 * root cause in a different layer, it stays open, and nothing in this correction
 * touched it.
 */
@RunWith(AndroidJUnit4.class)
public final class Ui3dStateCorrectionTest {

    /** The mechanical tolerance for an anchored surface, in dp. `UI3DC1-03`. */
    private static final float TOLERANCE_DP = 4.0f;

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private float density;

    @Before
    public void startFromTheBaseline() {
        resetToBaselineConstruction(rule.getScenario());
        density = onWorkspace(rule.getScenario(),
                (activity, workspace) -> activity.getResources().getDisplayMetrics().density);
    }

    @After
    public void leaveTheBaselineBehind() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.setBodyDimensionsMode(false);
            NativeViewport.sketchCancel();
            NativeViewport.supportChooserCancel();
            NativeViewport.enterConstructionMode();
            return null;
        });
        resetToBaselineConstruction(rule.getScenario());
    }

    // =======================================================================
    // UI3DC1-02 / UI3DC1-03 — the coordinate-space contract, and F-001 closed
    // =======================================================================

    /**
     * The conversion is runtime geometry and carries no inset constant.
     *
     * <p>Proved from the two ends rather than by reading the source: the overlay
     * the labels live in is genuinely inset-padded on this device (so the bug
     * had something to be wrong ABOUT), and a placed label nevertheless stands
     * on its anchor. If a constant had been subtracted instead, this would pass
     * only while the padding happened to equal it.
     */
    @Test
    public void ui3dc1_02_theOverlayIsPaddedAndTheLabelIsStillOnItsAnchor() {
        openDimensions();
        final int[] padding = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View labels = workspace.findViewById(R.id.body_dimension_labels);
            assertNotNull("the dimension labels container exists", labels);
            final View overlay = (View) labels.getParent();
            return new int[]{overlay.getPaddingLeft(), overlay.getPaddingTop(),
                    overlay.getPaddingRight(), overlay.getPaddingBottom()};
        });
        assertTrue("the overlay this chrome lives in really is inset-padded, so the"
                        + " conversion is doing work: " + padding[0] + "," + padding[1] + ","
                        + padding[2] + "," + padding[3],
                padding[0] + padding[1] + padding[2] + padding[3] > 0);
        assertAllDimensionLabelsAttached("dimensions_open");
    }

    /**
     * `UI3DC1-03`. No systematic offset survives, in either axis.
     *
     * <p>The audit's signature was a constant +128 px in Y on 30 of 60 measured
     * rows. Asserted per axis as well as by distance, so a fix that merely
     * shortened the error could not pass.
     */
    @Test
    public void ui3dc1_03_noSystematicOffsetRemains() {
        openDimensions();
        int measured = 0;
        for (int axis = 0; axis < 3; axis++) {
            final float[] error = dimensionLabelError(axis);
            if (error == null) {
                continue;
            }
            measured++;
            assertTrue("axis " + axis + " carries no systematic vertical offset: dy="
                            + (error[1] / density) + " dp",
                    Math.abs(error[1]) / density <= TOLERANCE_DP);
            assertTrue("axis " + axis + " carries no systematic horizontal offset: dx="
                            + (error[0] / density) + " dp",
                    Math.abs(error[0]) / density <= TOLERANCE_DP);
        }
        assertTrue("at least one axis was measurable and unclamped", measured > 0);
    }

    // =======================================================================
    // UI3DC1-04 — F-006: the clamp is against the viewport, not a padded box
    // =======================================================================

    /**
     * Every anchored surface stays inside the real viewport rectangle.
     *
     * <p>The correction moved the clamp bound from the container's padded
     * content box to the viewport carried into the same space. Both halves are
     * asserted: nothing leaves the viewport, and an UNCLAMPED surface — one
     * whose whole box fits at its anchor — is placed exactly on that anchor
     * rather than pushed in by an inset it did not need.
     */
    @Test
    public void ui3dc1_04_clampIsTheViewportAndAnUnclampedSurfaceIsNotPushedIn() {
        openDimensions();
        final int[] ids = {R.id.body_dimension_label_x, R.id.body_dimension_label_y,
                R.id.body_dimension_label_z};
        for (int axis = 0; axis < 3; axis++) {
            final int id = ids[axis];
            final boolean inside = onWorkspace(rule.getScenario(), (activity, workspace) -> {
                final View label = workspace.findViewById(id);
                final View viewport = workspace.findViewById(R.id.viewport_surface);
                if (label == null || !label.isShown()) {
                    return true;
                }
                final float[] centre = Ui3dAuditRecorder.centreOf(label, viewport);
                final float halfW = label.getWidth() * label.getScaleX() * 0.5f;
                final float halfH = label.getHeight() * label.getScaleY() * 0.5f;
                return centre[0] - halfW >= -0.5f && centre[1] - halfH >= -0.5f
                        && centre[0] + halfW <= viewport.getWidth() + 0.5f
                        && centre[1] + halfH <= viewport.getHeight() + 0.5f;
            });
            assertTrue("axis " + axis + " stays inside the true viewport rectangle", inside);
        }
        // Unclamped, so the clamp bound itself is what is under test: with the
        // padded box as the bound these same labels were pushed a whole inset in.
        assertAllDimensionLabelsAttached("clamp_bound");
    }

    // =======================================================================
    // UI3DC1-05 — F-003: present on the first frame of the owning mode
    // =======================================================================

    /**
     * Opening Dimensions is enough. No second, unrelated event is needed.
     *
     * <p>The audit found all three labels {@code GONE} with the mode open, the
     * body measurable and the anchors projecting, because the anchors only
     * became valid once the RENDER thread had built the overlay and the shell
     * never read again. They are now derived for the instant the chrome asks.
     */
    @Test
    public void ui3dc1_05_labelsAppearOnTheFrameTheModeOpens() {
        openDimensions();
        for (int axis = 0; axis < 3; axis++) {
            final int id = axis == 0 ? R.id.body_dimension_label_x
                    : axis == 1 ? R.id.body_dimension_label_y : R.id.body_dimension_label_z;
            assertTrue("axis " + axis + " is on screen with no second event",
                    shown(id));
        }
        assertAllDimensionLabelsAttached("first_frame");
    }

    // =======================================================================
    // UI3DC1-06 — F-002: the labels follow the camera
    // =======================================================================

    /** Orbit, pan and zoom each leave every label on its current anchor. */
    @Test
    public void ui3dc1_06_labelsFollowOrbitPanAndZoom() {
        openDimensions();
        orbitViewport();
        assertAllDimensionLabelsAttached("after_orbit");
        panViewport();
        assertAllDimensionLabelsAttached("after_pan");
        pinchViewport(true);
        assertAllDimensionLabelsAttached("after_zoom_out");
        pinchViewport(false);
        assertAllDimensionLabelsAttached("after_zoom_in");
    }

    // =======================================================================
    // UI3DC1-07 — the labels follow the body
    // =======================================================================

    /** A Move, a Rotate and a Scale each re-place the numbers they measure. */
    @Test
    public void ui3dc1_07_labelsFollowMoveRotateAndScale() {
        openDimensions();
        applyTransform(1.4, 0.6, -0.9, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
        assertAllDimensionLabelsAttached("after_move");
        applyTransform(1.4, 0.6, -0.9, 21.0, -37.0, 14.0, 1.0, 1.0, 1.0);
        assertAllDimensionLabelsAttached("after_rotate");
        applyTransform(1.4, 0.6, -0.9, 21.0, -37.0, 14.0, 1.8, 0.7, 1.3);
        assertAllDimensionLabelsAttached("after_scale");
    }

    // =======================================================================
    // UI3DC1-08 — F-007: the owner switch
    // =======================================================================

    /**
     * After A -> B the numbers describe B and stand on B, or they are gone.
     *
     * <p>The audit's worst measurement was 282 dp: the three labels stood at the
     * previous body's dimension-line midpoints while the mode measured the new
     * one. The assertion is deliberately "current owner or withdrawn" and never
     * "unchanged", because which of the two is right is the domain's call.
     */
    @Test
    public void ui3dc1_08_theLabelsNeverStandOnThePreviousBody() {
        final long first = activeBody();
        final long second = addSecondBodyApartFrom();
        assertTrue("a second body was created", second != NativeViewport.NO_OBJECT
                && second != first);

        selectBody(second);
        openDimensions();
        assertAllDimensionLabelsAttached("body_b");

        selectBody(first);
        syncChrome();
        settleLayout();
        // Whatever the mode decided, no number may stand at the OTHER body's
        // anchor: either the labels describe the body now measured, or they are
        // withdrawn.
        assertAllDimensionLabelsAttached("switched_to_body_a");
    }

    // =======================================================================
    // UI3DC1-09 — F-004: context invalidation
    // =======================================================================

    /** Hiding the owner withdraws its numbers on the same act. */
    @Test
    public void ui3dc1_09a_hidingTheOwnerWithdrawsTheLabels() {
        openDimensions();
        assertTrue("the labels are up before the body is hidden",
                shown(R.id.body_dimension_label_x));
        final long body = activeBody();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.sceneSetBodyVisible(body, false);
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        assertNoDimensionChrome("hidden owner");
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.sceneSetBodyVisible(body, true);
            workspace.onNativeStateChanged();
            return null;
        });
    }

    /** Entering Sculpt withdraws Construction-only anchored chrome. */
    @Test
    public void ui3dc1_09b_sculptDoesNotInheritGhostLabels() {
        openDimensions();
        assertTrue("the labels are up before Sculpt", shown(R.id.body_dimension_label_x));
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("the baseline body froze into a sculpt mesh", NativeViewport.SCULPT_OK,
                    NativeViewport.freezeToSculpt());
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        assertEquals("Sculpt is the mode", NativeViewport.MODE_SCULPT, NativeViewport.productMode());
        assertNoDimensionChrome("sculpt session");
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.enterConstructionMode();
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        assertNoDimensionChrome("back to Construction");
    }

    /** Closing the mode withdraws them, and no later refresh brings them back. */
    @Test
    public void ui3dc1_09c_closingTheModeClearsTheCachedOwner() {
        openDimensions();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.setBodyDimensionsMode(false);
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        assertNoDimensionChrome("mode closed");
        // The stale-owner half: an unrelated later refresh must not resurrect
        // chrome whose owner was cleared.
        orbitViewport();
        syncChrome();
        settleLayout();
        assertNoDimensionChrome("after an unrelated refresh");
    }

    // =======================================================================
    // UI3DC1-10 — Delete / Undo / Redo owner safety
    // =======================================================================

    /** A deleted owner takes its numbers with it, and an Undo does not ghost. */
    @Test
    public void ui3dc1_10_deleteAndUndoLeaveNoGhostChrome() {
        final long first = activeBody();
        final long second = addSecondBodyApartFrom();
        selectBody(second);
        openDimensions();
        assertTrue("the labels are up for the body about to go",
                shown(R.id.body_dimension_label_x));

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.sceneDeleteBody(second);
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        // Delete moves the selection to another body, and Dimensions measures
        // whichever body is active — so the contract is not "gone" but "never
        // the deleted one". Every number now on screen must stand on the CURRENT
        // owner's anchors, which is what a stale-owner defect cannot do.
        assertTrue("the deleted body is no longer the one being measured",
                activeBody() != second);
        assertAllDimensionLabelsAttached("after delete");

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.constructionUndo();
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        assertAllDimensionLabelsAttached("after undo");

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.constructionRedo();
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        assertAllDimensionLabelsAttached("after redo");
    }

    // =======================================================================
    // UI3DC1-11 — the CAD positive control, not regressed
    // =======================================================================

    /**
     * The CAD canvas chrome still tracks, and now without the constant offset.
     *
     * <p>The extrude cluster was the one surface that already refreshed on a
     * viewport gesture, so this asserts what the audit found CORRECT is still
     * correct — through Finish Sketch, an orbit and a zoom — and additionally
     * that the {@code UI3D-F-001} constant it also carried is gone. The
     * camera-attached scale is read back and held to its authored floor, because
     * a coordinate fix must not touch how big the cluster is drawn.
     */
    @Test
    public void ui3dc1_11_cadExtrudeClusterTracksAndKeepsItsAuthoredScale() {
        beginSketchXy();
        selectTool(rule.getScenario(), R.id.tool_rail_rectangle);
        dragSketch(rule.getScenario(), -0.6, -0.4, 0.6, 0.4);
        press(R.id.finish_sketch);
        assertEquals("Finish Sketch reached Ready", NativeViewport.SKETCH_READY, sketchState());
        settleLayout();

        assertExtrudeClusterAttached("sketch_ready");
        final float restingScale = extrudeScale();
        assertTrue("the camera-attached scale is inside its authored band: " + restingScale,
                restingScale >= 0.3999f && restingScale <= 1.6001f);

        orbitViewport();
        assertExtrudeClusterAttached("after_orbit");
        pinchViewport(true);
        assertExtrudeClusterAttached("after_zoom_out");
        assertTrue("the scale saturates at the authored 0.40 floor and never below it"
                        + " (CAD-FOUNDATION-C1 lowered it from 0.80)",
                extrudeScale() >= 0.3999f);
        pinchViewport(false);
        assertExtrudeClusterAttached("after_zoom_in");

        // Two Sides, so the SECOND value's own anchor is exercised too. The
        // three choices stand in the action palette the one panel opens
        // (`CAD-FOUNDATION-C2`).
        press(R.id.cad_extrude_panel);
        press(R.id.cad_extrude_extent_two_sides);
        settleLayout();
        assertExtrudeClusterAttached("two_sides");

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.sketchCancel();
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    // =======================================================================
    // Assertions
    // =======================================================================

    /**
     * Every dimension label that is on screen stands on its own current anchor.
     *
     * <p>Also asserts the converse, which is the ghost-UI half: a label that is
     * SHOWN while its anchor does not project is chrome standing at a guess.
     */
    private void assertAllDimensionLabelsAttached(String where) {
        settleLayout();
        final int[] ids = {R.id.body_dimension_label_x, R.id.body_dimension_label_y,
                R.id.body_dimension_label_z};
        int measured = 0;
        for (int axis = 0; axis < 3; axis++) {
            final int id = ids[axis];
            final float[] expected = new float[2];
            final boolean projects = NativeViewport.bodyDimensionLabelPoint(axis, expected);
            final float[] centre = centreOf(id);
            if (centre == null) {
                continue;
            }
            assertTrue(where + ": axis " + axis + " is shown although its anchor does not"
                    + " project, so it stands at a guess", projects);
            // A label whose whole box does not fit at its anchor is deliberately
            // held inside the viewport, so its distance from the anchor is
            // correct rather than stale. Classified exactly as the audit
            // classifies it, and the clamp itself is asserted by UI3DC1-04.
            if (wouldClamp(id, expected[0], expected[1])) {
                continue;
            }
            measured++;
            final float dx = centre[0] - expected[0];
            final float dy = centre[1] - expected[1];
            final float dp = (float) Math.sqrt(dx * dx + dy * dy) / density;
            assertTrue(where + ": axis " + axis + " stands " + dp + " dp from its anchor"
                            + " (expected " + expected[0] + "," + expected[1]
                            + " actual " + centre[0] + "," + centre[1] + ")",
                    dp <= TOLERANCE_DP);
        }
        assertTrue(where + ": at least one label was measurable and unclamped, so the"
                + " assertion is not vacuous", measured > 0 || !shown(ids[0]));
    }

    /** Whether an anchor is close enough to a viewport edge that clamping bites. */
    private boolean wouldClamp(final int id, final float anchorX, final float anchorY) {
        return Boolean.TRUE.equals(onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View shown = workspace.findViewById(id);
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            if (shown == null || viewport == null) {
                return false;
            }
            return Ui3dAuditRecorder.wouldClamp(shown, viewport, anchorX, anchorY);
        }));
    }

    /** Neither the labels nor the editor survive into a state that refuses them. */
    private void assertNoDimensionChrome(String where) {
        assertFalse(where + ": the X label survives", shown(R.id.body_dimension_label_x));
        assertFalse(where + ": the Y label survives", shown(R.id.body_dimension_label_y));
        assertFalse(where + ": the Z label survives", shown(R.id.body_dimension_label_z));
        assertFalse(where + ": the dimension editor survives", shown(R.id.body_dimension_editor));
    }

    /**
     * The CAD extrude value stands on the leader native reports for it
     * (`CAD-FOUNDATION-C1`): rotated to the leader, above it, along its visible
     * part, as an unscaled 48 dp proxy.
     */
    private void assertExtrudeClusterAttached(String where) {
        settleLayout();
        final double[] tool = new double[NativeViewport.CAD_EXTRUDE_SIZE];
        NativeViewport.cadExtrudeToolState(tool);
        if (tool[NativeViewport.CAD_EXTRUDE_ACTIVE] == 0.0
                || tool[NativeViewport.CAD_EXTRUDE_ON_SCREEN] == 0.0
                || CadLeaderHudChecks.leader(tool, false) == null) {
            return;
        }
        final String why = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final android.widget.TextView value =
                    workspace.findViewById(R.id.cad_extrude_depth_value);
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final CadExtrudeCanvasView canvas = workspace.cadExtrudeCanvas();
            final View dock = canvas.findViewById(R.id.cad_extrude_panel);
            // The value collapses at the scale floor when it would outgrow its
            // leader; the dock obeys native alone
            // (`CAD-V6-S2-OWNER-CORRECTION-E2E-R1`).
            if (canvas.lastAnnotationCollapsed()) {
                return value.isShown() ? "a collapsed value is still shown" : null;
            }
            // The dock, when native draws it, stands WHOLE past the arrow's point.
            if (canvas.lastDock().visible) {
                final String panel = CadLeaderHudChecks.dockAtArrow(tool, canvas, viewport,
                        density);
                if (panel != null) {
                    return panel;
                }
            } else if (dock.isShown()) {
                return "the dock is shown while native hides it";
            }
            final float[] l = CadLeaderHudChecks.leader(tool, false);
            if (CadHudPresentation.clipToViewport(l[0], l[1], l[2], l[3], viewport.getWidth(),
                    viewport.getHeight()) == null) {
                return value.isShown() ? "a value is shown for a leader wholly off screen" : null;
            }
            return CadLeaderHudChecks.valueOnLeader(tool, value, viewport, density, false);
        });
        assertTrue(where + ": the extrude value stands on its leader: " + why, why == null);
    }

    // =======================================================================
    // Reads
    // =======================================================================

    /** The signed error of one UNCLAMPED dimension label, in pixels, or null. */
    private float[] dimensionLabelError(int axis) {
        settleLayout();
        final float[] expected = new float[2];
        if (!NativeViewport.bodyDimensionLabelPoint(axis, expected)) {
            return null;
        }
        final int id = axis == 0 ? R.id.body_dimension_label_x
                : axis == 1 ? R.id.body_dimension_label_y : R.id.body_dimension_label_z;
        if (wouldClamp(id, expected[0], expected[1])) {
            return null;
        }
        final float[] centre = centreOf(id);
        return centre == null ? null
                : new float[]{centre[0] - expected[0], centre[1] - expected[1]};
    }

    /** The visual centre of one chrome view in viewport pixels, or null. */
    private float[] centreOf(final int id) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View shown = workspace.findViewById(id);
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            if (shown == null || !shown.isShown() || viewport == null) {
                return null;
            }
            return Ui3dAuditRecorder.centreOf(shown, viewport);
        });
    }

    private float extrudeScale() {
        final double[] tool = new double[NativeViewport.CAD_EXTRUDE_SIZE];
        NativeViewport.cadExtrudeToolState(tool);
        return (float) tool[NativeViewport.CAD_EXTRUDE_SCALE];
    }

    private boolean shown(final int id) {
        return Boolean.TRUE.equals(onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View view = workspace.findViewById(id);
            return view != null && view.isShown();
        }));
    }

    private long activeBody() {
        return onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.sceneActiveBodyId());
    }

    // =======================================================================
    // Journey
    // =======================================================================

    /** Transform -> Dimensions, asserted to have actually opened. */
    private void openDimensions() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.uiState().setConstructionTool(EditorUiState.CONSTRUCTION_TOOL_TRANSFORM);
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("Dimensions mode opened", NativeViewport.setBodyDimensionsMode(true));
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    /** A second Construction Body, standing well clear of the first. */
    private long addSecondBodyApartFrom() {
        final long created = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final long id = NativeViewport.sceneAddBody();
            if (id != NativeViewport.NO_OBJECT) {
                NativeViewport.applyConstructionBox(1.0, 1.0, 1.0);
                NativeViewport.applyBoxTransform(1.6, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
            }
            workspace.onNativeStateChanged();
            return id;
        });
        settleLayout();
        return created;
    }

    private void selectBody(final long id) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.sceneSelectBody(id);
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    private void applyTransform(final double px, final double py, final double pz,
                                final double rx, final double ry, final double rz,
                                final double sx, final double sy, final double sz) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.applyBoxTransform(px, py, pz, rx, ry, rz, sx, sy, sz);
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    private void beginSketchXy() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("a world-plane sketch began", NativeViewport.CAD_OK,
                    NativeViewport.sketchBegin(NativeViewport.WORKPLANE_XY));
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    private void syncChrome() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.onNativeStateChanged();
            return null;
        });
        settleLayout();
    }

    private void press(final int id) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View view = workspace.findViewById(id);
            assertNotNull("control " + id + " exists", view);
            assertTrue("control " + id + " is on screen", view.isShown());
            view.performClick();
            return null;
        });
        settleLayout();
    }

    // =======================================================================
    // Real viewport gestures
    //
    // Sent to the surface itself, so they travel the same path a finger does and
    // exercise the very refresh drivers under test. Nothing here reaches the
    // domain except through the product's own pointer boundary.
    // =======================================================================

    private void orbitViewport() {
        final float[] centre = viewportCentre();
        dragViewport(centre[0] - 120f, centre[1] - 60f, centre[0] + 120f, centre[1] + 60f);
    }

    private void panViewport() {
        final float[] centre = viewportCentre();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View target = workspace.findViewById(R.id.viewport_surface);
            final long down = SystemClock.uptimeMillis();
            sendTwo(target, down, down, MotionEvent.ACTION_DOWN, centre[0] - 60f, centre[1],
                    centre[0] + 60f, centre[1], 1);
            sendTwo(target, down, down + 5, MotionEvent.ACTION_POINTER_DOWN
                    | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT), centre[0] - 60f, centre[1],
                    centre[0] + 60f, centre[1], 2);
            for (int step = 1; step <= 6; step++) {
                final float shift = 14f * step;
                sendTwo(target, down, down + 5 + step * 8L, MotionEvent.ACTION_MOVE,
                        centre[0] - 60f + shift, centre[1] + shift,
                        centre[0] + 60f + shift, centre[1] + shift, 2);
            }
            sendTwo(target, down, down + 70, MotionEvent.ACTION_POINTER_UP
                    | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT), centre[0] - 60f + 84f,
                    centre[1] + 84f, centre[0] + 60f + 84f, centre[1] + 84f, 2);
            send(target, down, down + 75, MotionEvent.ACTION_UP, centre[0] - 60f + 84f,
                    centre[1] + 84f);
            return null;
        });
        settleLayout();
    }

    private void pinchViewport(final boolean out) {
        final float[] centre = viewportCentre();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View target = workspace.findViewById(R.id.viewport_surface);
            final long down = SystemClock.uptimeMillis();
            final float from = out ? 220f : 60f;
            final float to = out ? 60f : 220f;
            sendTwo(target, down, down, MotionEvent.ACTION_DOWN, centre[0] - from, centre[1],
                    centre[0] + from, centre[1], 1);
            sendTwo(target, down, down + 5, MotionEvent.ACTION_POINTER_DOWN
                    | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT), centre[0] - from, centre[1],
                    centre[0] + from, centre[1], 2);
            for (int step = 1; step <= 6; step++) {
                final float span = from + (to - from) * step / 6f;
                sendTwo(target, down, down + 5 + step * 8L, MotionEvent.ACTION_MOVE,
                        centre[0] - span, centre[1], centre[0] + span, centre[1], 2);
            }
            sendTwo(target, down, down + 70, MotionEvent.ACTION_POINTER_UP
                            | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT),
                    centre[0] - to, centre[1], centre[0] + to, centre[1], 2);
            send(target, down, down + 75, MotionEvent.ACTION_UP, centre[0] - to, centre[1]);
            return null;
        });
        settleLayout();
    }

    private void dragViewport(final float fromX, final float fromY, final float toX,
                              final float toY) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View target = workspace.findViewById(R.id.viewport_surface);
            final long down = SystemClock.uptimeMillis();
            send(target, down, down, MotionEvent.ACTION_DOWN, fromX, fromY);
            for (int step = 1; step <= 8; step++) {
                send(target, down, down + step * 8L, MotionEvent.ACTION_MOVE,
                        fromX + (toX - fromX) * step / 8f, fromY + (toY - fromY) * step / 8f);
            }
            send(target, down, down + 80, MotionEvent.ACTION_UP, toX, toY);
            return null;
        });
        settleLayout();
    }

    private float[] viewportCentre() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View target = workspace.findViewById(R.id.viewport_surface);
            return new float[]{target.getWidth() * 0.5f, target.getHeight() * 0.5f};
        });
    }

    private static void send(View target, long downTime, long eventTime, int action, float x,
                             float y) {
        final MotionEvent event = MotionEvent.obtain(downTime, eventTime, action, x, y, 0);
        event.setSource(android.view.InputDevice.SOURCE_TOUCHSCREEN);
        target.dispatchTouchEvent(event);
        event.recycle();
    }

    private static void sendTwo(View target, long downTime, long eventTime, int action, float x0,
                                float y0, float x1, float y1, int count) {
        final MotionEvent.PointerProperties[] properties = new MotionEvent.PointerProperties[count];
        final MotionEvent.PointerCoords[] positions = new MotionEvent.PointerCoords[count];
        for (int i = 0; i < count; i++) {
            properties[i] = new MotionEvent.PointerProperties();
            properties[i].id = i;
            properties[i].toolType = MotionEvent.TOOL_TYPE_FINGER;
            positions[i] = coords(i == 0 ? x0 : x1, i == 0 ? y0 : y1);
        }
        final MotionEvent event = MotionEvent.obtain(downTime, eventTime, action, count, properties,
                positions, 0, 0, 1f, 1f, 0, 0, android.view.InputDevice.SOURCE_TOUCHSCREEN, 0);
        target.dispatchTouchEvent(event);
        event.recycle();
    }

    private static MotionEvent.PointerCoords coords(float x, float y) {
        final MotionEvent.PointerCoords at = new MotionEvent.PointerCoords();
        at.x = x;
        at.y = y;
        at.pressure = 1f;
        at.size = 1f;
        return at;
    }
}
