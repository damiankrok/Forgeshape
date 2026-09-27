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

import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Point;
import android.graphics.Rect;
import android.os.Build;
import android.os.SystemClock;
import android.util.Log;
import android.view.MotionEvent;
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

/**
 * `STAGE027-SCULPT-ISOLATE-R1`: the Sculpt workflow's device journey.
 *
 * <p>Three things, each through the path a user takes — real MotionEvents on
 * the viewport surface, the toolbar transitions, the Objects-row Show/Hide
 * control and the Sculpt Property Inspector's Isolate control, all by semantic
 * id. Native setters only BUILD the scene a journey starts from.
 *
 * <ul>
 *   <li><b>GUARD-1</b> ({@code FINDING-A}, OWNER GUARD-1 = a): in Sculpt, a tap
 *       that starts no stroke changes nothing — not the active body, not the
 *       Sculpt target and not the viewport selection — while a real stroke on
 *       the target still lands and a drag off it still navigates.</li>
 *   <li><b>GUARD-2</b> ({@code FINDING-B}, OWNER GUARD-2 = a): over a hidden
 *       active body Start Sculpting and Resume Sculpt are ABSENT, the native
 *       entries refuse by name, and a refusal moves nothing.</li>
 *   <li><b>Isolate</b> (OWNER UI-1 = b, LIFE-1 = a): one two-state control in
 *       the Sculpt Property Inspector restricts the viewport's one list to the
 *       Sculpt target, measured in what the renderer is handed and in the
 *       captured frame, is not project truth, and clears on Back so Resume
 *       opens un-isolated.</li>
 * </ul>
 *
 * <p>Both defects were first REPRODUCED on the unfixed product by this class's
 * phase-0 form (commit 3de71ee, CI DEVICE run 36318528002) before either
 * guard was written.
 */
@RunWith(AndroidJUnit4.class)
public final class Stage027SculptWorkflowTest {

    private static final String TAG = "ForgeShape";

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private long bodyA;
    private long bodyB;

    @Before
    public void freshTwoBodyScene() {
        freshProject();
        final long[] ids = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final long a = NativeViewport.sceneActiveBodyId();
            assertNotEquals("the fresh project has a body", 0L, a);
            assertTransform(NativeViewport.applyConstructionSphere(1.0));
            final long b = NativeViewport.sceneAddBody();
            assertNotEquals("a second body can be added", 0L, b);
            assertTransform(NativeViewport.applyBoxTransform(2.5, 0.0, 0.0, 0.0, 0.0, 0.0,
                    1.0, 1.0, 1.0));
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(a));
            assertEquals("exactly the two bodies this journey is about", 2,
                    NativeViewport.sceneBodyCount());
            workspace.syncFromNative();
            return new long[] {a, b};
        });
        bodyA = ids[0];
        bodyB = ids[1];
        settleLayout();
    }

    @After
    public void leaveNothingBehind() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.setChromeHidden(false);
            return null;
        });
        // A hidden, isolated or sculpting body must not become the next
        // class's baseline.
        freshProject();
    }

    // -----------------------------------------------------------------------
    // GUARD-1
    // -----------------------------------------------------------------------

    @Test
    public void guard1_aNonStrokeSculptTapChangesNothing() {
        startSculptingThroughTheToolbar();
        final long selectionBefore = NativeViewport.debugViewportSelection();
        final double[] before = sculptState();
        assertEquals(bodyA, (long) before[NativeViewport.SCULPT_OBJECT_ID]);

        // A tap on the OTHER body -- the path that used to switch the target.
        final float[] onB = projected(2.5, 0.0, 0.0);
        final float[] onA = projected(0.0, 0.0, 0.0);
        assertTrue("B's centre is well clear of A's silhouette on screen",
                Math.hypot(onB[0] - onA[0], onB[1] - onA[1]) > 80.0);
        tapViewport(onB[0], onB[1]);
        assertNothingMoved("a tap on another body", before, selectionBefore);

        // A tap that misses everything.
        final float[] empty = emptyPixel();
        tapViewport(empty[0], empty[1]);
        assertNothingMoved("a tap on empty space", before, selectionBefore);

        // The row path still refuses the same act, unchanged.
        assertNotEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(bodyB));
        assertEquals(bodyA, NativeViewport.sceneActiveBodyId());

        // Navigation is untouched: a drag that starts off the sculpt mesh still
        // orbits the camera, and moves no vertex.
        final float[] poseBefore = cameraPose();
        dragViewport(empty[0], empty[1], empty[0] + 220f, empty[1] + 60f);
        final float[] poseAfter = cameraPose();
        assertTrue("a drag off the sculpt mesh still orbits",
                poseBefore[0] != poseAfter[0] || poseBefore[1] != poseAfter[1]);
        assertEquals("and moved no vertex", before[NativeViewport.SCULPT_REVISION],
                sculptState()[NativeViewport.SCULPT_REVISION], 0.0);

        // A real stroke on the Sculpt target still lands, on the target.
        final float[] onAAgain = projected(0.0, 0.0, 0.0);
        dragViewport(onAAgain[0], onAAgain[1], onAAgain[0] + 60f, onAAgain[1] + 40f);
        final double[] stroked = sculptState();
        assertTrue("a stroke on the target mints a sculpt revision",
                stroked[NativeViewport.SCULPT_REVISION] > before[NativeViewport.SCULPT_REVISION]);
        assertEquals("on the target", bodyA, (long) stroked[NativeViewport.SCULPT_OBJECT_ID]);
        assertEquals(bodyA, NativeViewport.sceneActiveBodyId());
    }

    // -----------------------------------------------------------------------
    // GUARD-2
    // -----------------------------------------------------------------------

    @Test
    public void guard2_aHiddenBodyCannotStartSculpting() {
        assertTrue("precondition: Start Sculpting is offered", isShown(R.id.freeze_to_sculpt));
        toggleVisibilityThroughTheObjectsRow(bodyA);
        assertFalse("precondition: A is hidden", NativeViewport.sceneBodyVisible(bodyA));

        assertFalse("Start Sculpting is withdrawn over a hidden body",
                isShown(R.id.freeze_to_sculpt));
        assertFalse("and so is Resume", isShown(R.id.resume_sculpt));

        // The native guard stands beneath the withdrawn control.
        final byte[] bytesBefore = NativeViewport.encodeProject();
        final long fingerprintBefore = NativeViewport.projectFingerprint();
        final int status = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.freezeToSculpt());
        final double[] after = sculptState();
        assertEquals("refused by name", NativeViewport.SCULPT_REFUSED_HIDDEN_BODY, status);
        assertEquals("the mode did not change", NativeViewport.MODE_CONSTRUCTION,
                NativeViewport.productMode());
        assertEquals("nothing was frozen", 0.0, after[NativeViewport.SCULPT_HAS_MESH], 0.0);
        assertFalse("the body is still hidden", NativeViewport.sceneBodyVisible(bodyA));
        assertArrayEquals("the document is byte-identical", bytesBefore,
                NativeViewport.encodeProject());
        assertEquals("the fingerprint is unmoved", fingerprintBefore,
                NativeViewport.projectFingerprint());

        // Showing the body again brings the control back.
        toggleVisibilityThroughTheObjectsRow(bodyA);
        assertTrue(NativeViewport.sceneBodyVisible(bodyA));
        assertTrue("Start Sculpting returns over a visible body", isShown(R.id.freeze_to_sculpt));
    }

    @Test
    public void guard2_aHiddenSculptBodyCannotResume() {
        startSculptingThroughTheToolbar();
        clickToolbar(R.id.back_to_construction);
        assertEquals(NativeViewport.MODE_CONSTRUCTION, NativeViewport.productMode());
        assertTrue("precondition: Resume is offered", isShown(R.id.resume_sculpt));
        final double revisionBefore = sculptState()[NativeViewport.SCULPT_REVISION];

        toggleVisibilityThroughTheObjectsRow(bodyA);
        assertFalse(NativeViewport.sceneBodyVisible(bodyA));
        assertFalse("Resume Sculpt is withdrawn over a hidden body", isShown(R.id.resume_sculpt));
        assertFalse("and Start Sculpting is not offered in its place",
                isShown(R.id.freeze_to_sculpt));

        final long fingerprintBefore = NativeViewport.projectFingerprint();
        final int status = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.enterSculptMode());
        assertEquals("refused by name", NativeViewport.SCULPT_REFUSED_HIDDEN_BODY, status);
        assertEquals("the mode did not change", NativeViewport.MODE_CONSTRUCTION,
                NativeViewport.productMode());
        assertEquals("the sculpt mesh is untouched", revisionBefore,
                sculptState()[NativeViewport.SCULPT_REVISION], 0.0);
        assertFalse(NativeViewport.sceneBodyVisible(bodyA));
        assertEquals(fingerprintBefore, NativeViewport.projectFingerprint());

        toggleVisibilityThroughTheObjectsRow(bodyA);
        assertTrue("Resume returns over a visible body", isShown(R.id.resume_sculpt));
    }

    // -----------------------------------------------------------------------
    // Isolate
    // -----------------------------------------------------------------------

    @Test
    public void isolate_theInspectorControlRestrictsTheViewportAndIsNotTruth() {
        // Absent outside Sculpt.
        openPrecision();
        assertFalse("no Isolate control in Construction", isShown(R.id.sculpt_isolate));
        closePrecision();

        startSculptingThroughTheToolbar();
        openPrecision();
        assertTrue("the Isolate control is in the Sculpt Property Inspector",
                isShown(R.id.sculpt_isolate));
        assertIsolateControl(false);
        assertArrayEquals("un-isolated, the viewport's list is both bodies",
                new long[] {bodyA, bodyB}, viewSceneIds());

        // A fixed, stated viewpoint, so the two bodies' pixels are a property
        // of this test rather than of whatever pose a previous test left. At
        // the product's INITIAL pose (distance 8.2) B's centre projects to
        // viewport x ~1126 on a 1080-wide portrait surface -- outside the
        // frame, which is exactly how an earlier form of this test sampled a
        // region with zero pixels in it. From 12 m on the initial yaw and
        // pitch, A projects near (540, 1200), B near (915, 1352) and the empty
        // reference spot near (244, 1080). The camera is presentation, so the
        // pose is set BEFORE the truth baseline below and restored after.
        final float[] poseBefore = cameraPose();
        setCameraPose(0.7f, 0.5f, 12.0f);
        try {
            measureIsolateFrames();
        } finally {
            setCameraPose(poseBefore[NativeViewport.CAMERA_POSE_YAW],
                    poseBefore[NativeViewport.CAMERA_POSE_PITCH],
                    poseBefore[NativeViewport.CAMERA_POSE_DISTANCE]);
        }
    }

    /**
     * The Isolate journey from the frame before to the Resume after, with the
     * captured-frame proof in the middle. Every sampled square is converted
     * EXPLICITLY into the captured bitmap's own pixels and must lie wholly
     * inside it; nothing here clips, and nothing here has a path that passes
     * without measuring.
     */
    private void measureIsolateFrames() {
        final byte[] bytesBefore = NativeViewport.encodeProject();
        final long fingerprintBefore = NativeViewport.projectFingerprint();
        final int sculptUndoBefore = NativeViewport.sculptUndoDepth();
        final double revisionBefore = sculptState()[NativeViewport.SCULPT_REVISION];

        // Where the three squares stand, in the surface's own pixels (the
        // space debugProjectWorld answers in). The reference spot is a world
        // point no body can cover: B stands at +2.5 m and A's radius is 1 m.
        final float[] atA = projected(0.0, 0.0, 0.0);
        final float[] atB = projected(2.5, 0.0, 0.0);
        final float[] atEmpty = projected(-2.5, 0.0, 0.0);
        final CaptureGeometry geometry = captureGeometry(atA, atB, atEmpty);

        final Bitmap off = captureBareViewport();
        // The controlled differential: the same frame captured twice with
        // nothing changed. Its per-region change is the noise floor every
        // "unchanged" claim below is held to.
        final Bitmap offAgain = captureBareViewport();
        final Sample offA = geometry.sample(off, geometry.a, "offA");
        final Sample offB = geometry.sample(off, geometry.b, "offB");
        final Sample offEmpty = geometry.sample(off, geometry.empty, "offEmpty");
        final double noiseA = geometry.changedFraction(off, offAgain, geometry.a);
        final double noiseB = geometry.changedFraction(off, offAgain, geometry.b);
        final double noiseEmpty = geometry.changedFraction(off, offAgain, geometry.empty);
        final int contrastOffA = offA.distanceTo(offEmpty);
        final int contrastOffB = offB.distanceTo(offEmpty);
        Log.i(TAG, "STAGE027_PIXEL_OFF " + geometry.describe(off) + " meanA=" + offA
                + " meanB=" + offB + " meanEmpty=" + offEmpty + " contrastA=" + contrastOffA
                + " contrastB=" + contrastOffB + " noiseA=" + noiseA + " noiseB=" + noiseB
                + " noiseEmpty=" + noiseEmpty);
        final String offWhere = " [" + geometry.describe(off) + "]";
        assertTrue("the un-isolated frame is stable: noise " + noiseA + "/" + noiseB + "/"
                        + noiseEmpty + offWhere,
                noiseA <= kStableFraction && noiseB <= kStableFraction
                        && noiseEmpty <= kStableFraction);
        assertTrue("B is drawn in its square before Isolate: contrast " + contrastOffB
                + " (B " + offB + ", empty " + offEmpty + ")" + offWhere,
                contrastOffB >= kBodySignal);
        assertTrue("A is drawn in its square before Isolate: contrast " + contrastOffA
                + " (A " + offA + ", empty " + offEmpty + ")" + offWhere,
                contrastOffA >= kBodySignal);

        clickIsolate();
        assertTrue("native reports the viewport isolated", NativeViewport.sculptIsolated());
        assertIsolateControl(true);
        assertArrayEquals("isolated, the viewport's list is the Sculpt target alone",
                new long[] {bodyA}, viewSceneIds());

        final Bitmap on = captureBareViewport();
        final Sample onA = geometry.sample(on, geometry.a, "onA");
        final Sample onB = geometry.sample(on, geometry.b, "onB");
        final Sample onEmpty = geometry.sample(on, geometry.empty, "onEmpty");
        final double changedAtA = geometry.changedFraction(off, on, geometry.a);
        final double changedAtB = geometry.changedFraction(off, on, geometry.b);
        final double changedAtEmpty = geometry.changedFraction(off, on, geometry.empty);
        final int contrastOnA = onA.distanceTo(onEmpty);
        final int contrastOnB = onB.distanceTo(onEmpty);
        Log.i(TAG, "STAGE027_PIXEL_ON " + geometry.describe(on) + " meanA=" + onA
                + " meanB=" + onB + " meanEmpty=" + onEmpty + " contrastA=" + contrastOnA
                + " contrastB=" + contrastOnB + " changedA=" + changedAtA + " changedB="
                + changedAtB + " changedEmpty=" + changedAtEmpty);
        final String onWhere = " [" + geometry.describe(on) + "]";
        // B: its square changed almost everywhere, and what is there now reads
        // as the empty ground rather than as a body.
        assertTrue("B is no longer drawn in its square: changed " + changedAtB + onWhere,
                changedAtB >= kGoneFraction);
        assertTrue("B's square now reads as empty ground: contrast " + contrastOnB
                        + " against " + contrastOffB + " before (B " + onB + ", empty "
                        + onEmpty + ")" + onWhere,
                contrastOnB * kGoneContrastRatio <= contrastOffB);
        // A: unchanged to within the measured noise, and still a body -- so a
        // frame in which everything vanished cannot pass.
        assertTrue("A is still drawn exactly as before: changed " + changedAtA + ", noise "
                + noiseA + onWhere, changedAtA <= noiseA + kStableFraction);
        assertTrue("A is still drawn in its square: contrast " + contrastOnA + " (A " + onA
                + ", empty " + onEmpty + ")" + onWhere, contrastOnA >= kBodySignal);
        // The empty spot is the control: Isolate does not repaint the ground.
        assertTrue("the empty ground is unchanged: changed " + changedAtEmpty + ", noise "
                        + noiseEmpty + onWhere,
                changedAtEmpty <= noiseEmpty + kStableFraction);

        // A view decision, not truth: no byte, no fingerprint, no step, no
        // revision, no visibility write.
        assertArrayEquals("the document is byte-identical", bytesBefore,
                NativeViewport.encodeProject());
        assertEquals("the fingerprint is unmoved", fingerprintBefore,
                NativeViewport.projectFingerprint());
        assertEquals("no sculpt history entry", sculptUndoBefore, NativeViewport.sculptUndoDepth());
        assertEquals("no sculpt revision", revisionBefore,
                sculptState()[NativeViewport.SCULPT_REVISION], 0.0);
        assertTrue("B is still durably visible", NativeViewport.sceneBodyVisible(bodyB));

        // Ordinary navigation keeps it.
        final float[] empty = emptyPixel();
        dragViewport(empty[0], empty[1], empty[0] + 160f, empty[1] + 40f);
        assertTrue("an orbit does not clear the isolate", NativeViewport.sculptIsolated());

        // No world-anchored chrome is left standing for the excluded body.
        assertNoAnchoredChromeShown();

        // Off again restores the ordinary multi-body view.
        clickIsolate();
        assertFalse(NativeViewport.sculptIsolated());
        assertIsolateControl(false);
        assertArrayEquals(new long[] {bodyA, bodyB}, viewSceneIds());

        // LIFE-1 = clear: Back leaves nothing isolated, and Resume opens off.
        clickIsolate();
        assertTrue(NativeViewport.sculptIsolated());
        clickToolbar(R.id.back_to_construction);
        assertFalse("Back to Construction clears the isolate", NativeViewport.sculptIsolated());
        assertArrayEquals("Construction is never drawn isolated",
                new long[] {bodyA, bodyB}, viewSceneIds());
        clickToolbar(R.id.resume_sculpt);
        assertEquals(NativeViewport.MODE_SCULPT, NativeViewport.productMode());
        assertFalse("Resume opens un-isolated", NativeViewport.sculptIsolated());
        openPrecision();
        assertIsolateControl(false);
        assertArrayEquals(new long[] {bodyA, bodyB}, viewSceneIds());
    }

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    private void assertNothingMoved(String what, double[] before, long selectionBefore) {
        final double[] after = sculptState();
        assertEquals(what + " leaves the active body", bodyA, NativeViewport.sceneActiveBodyId());
        assertEquals(what + " leaves the Sculpt target", bodyA,
                (long) after[NativeViewport.SCULPT_OBJECT_ID]);
        assertEquals(what + " leaves Sculpt on", NativeViewport.MODE_SCULPT,
                (int) after[NativeViewport.SCULPT_MODE]);
        assertEquals(what + " leaves the viewport selection", selectionBefore,
                NativeViewport.debugViewportSelection());
        assertEquals(what + " moves no vertex", before[NativeViewport.SCULPT_REVISION],
                after[NativeViewport.SCULPT_REVISION], 0.0);
        assertEquals(what + " records no stroke", before[NativeViewport.SCULPT_STROKE_COUNT],
                after[NativeViewport.SCULPT_STROKE_COUNT], 0.0);
    }

    private void assertIsolateControl(final boolean isolated) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final TextView control = workspace.findViewById(R.id.sculpt_isolate);
            assertNotNull("the Isolate control exists", control);
            assertEquals("the label names the next act",
                    activity.getString(isolated ? R.string.sculpt_isolate_exit
                                                : R.string.sculpt_isolate),
                    control.getText().toString());
            assertEquals("the description states the state, not colour alone",
                    activity.getString(isolated ? R.string.sculpt_isolate_exit_description
                                                : R.string.sculpt_isolate_description),
                    String.valueOf(control.getContentDescription()));
            final float density = activity.getResources().getDisplayMetrics().density;
            final int floor = Math.round(48f * density);
            assertTrue("the control meets the 48 dp floor: " + control.getHeight(),
                    control.getHeight() >= floor - 1);
            return null;
        });
    }

    private void assertNoAnchoredChromeShown() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int[] anchored = {R.id.body_dimension_labels, R.id.sketch_dimension_label,
                    R.id.sketch_orientation_navigator, R.id.cad_extrude_canvas};
            for (int id : anchored) {
                final View view = workspace.findViewById(id);
                assertTrue("no world-anchored chrome stands in Sculpt: " + id,
                        view == null || !view.isShown());
            }
            return null;
        });
    }

    private void clickIsolate() {
        openPrecision();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View control = workspace.findViewById(R.id.sculpt_isolate);
            assertNotNull("the Isolate control exists", control);
            assertTrue("the Isolate control is on screen", control.isShown());
            control.performClick();
            return null;
        });
        settleLayout();
    }

    private void openPrecision() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openPrecision(workspace);
            return null;
        });
        settleLayout();
    }

    private void closePrecision() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.closePrecision(workspace);
            return null;
        });
        settleLayout();
    }

    /**
     * The composed frame with the chrome hidden, so nothing but the viewport is
     * measured. Hiding the chrome closes the precision surface, so it is
     * reopened afterwards the way a user reopens it.
     */
    private Bitmap captureBareViewport() {
        hideChrome(true);
        final Bitmap shot = capture();
        hideChrome(false);
        openPrecision();
        return shot;
    }

    private void hideChrome(final boolean hidden) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.setChromeHidden(hidden);
            return null;
        });
        settleLayout();
    }

    /** A new, unsaved Construction project; closing writes nothing. */
    private void freshProject() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.enterConstructionMode();
            NativeViewport.sketchCancel();
            NativeViewport.supportChooserCancel();
            NativeViewport.closeProject();
            return null;
        });
        resetToBaselineConstruction(rule.getScenario());
    }

    private void startSculptingThroughTheToolbar() {
        assertTrue("Start Sculpting is offered", isShown(R.id.freeze_to_sculpt));
        clickToolbar(R.id.freeze_to_sculpt);
        assertEquals("Start Sculpting entered Sculpt", NativeViewport.MODE_SCULPT,
                NativeViewport.productMode());
    }

    private boolean isShown(final int id) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View control = workspace.findViewById(id);
            return control != null && control.isShown();
        });
    }

    private void clickToolbar(final int id) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View control = workspace.findViewById(id);
            assertNotNull("the control exists in the workspace", control);
            assertTrue("the control is on screen", control.isShown());
            control.performClick();
            return null;
        });
        settleLayout();
    }

    /** Show/Hide through the Objects row's own control, as a user does it. */
    private void toggleVisibilityThroughTheObjectsRow(final long objectId) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openObjectsPanel(workspace);
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            if (!Long.valueOf(objectId).equals(workspace.objectsSection().expandedRow())) {
                final View more = find(workspace, R.id.object_row_more, objectId);
                assertNotNull("the row offers its overflow", more);
                more.performClick();
            }
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View visibility = find(workspace, R.id.object_row_visibility, objectId);
            assertNotNull("the row offers Show/Hide", visibility);
            visibility.performClick();
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.closeObjectsPanel(workspace);
            return null;
        });
        settleLayout();
    }

    private static View find(EditorWorkspaceView workspace, int id, long objectId) {
        return search(workspace.objectsSection(), id, Long.valueOf(objectId));
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

    private static double[] sculptState() {
        final double[] state = new double[NativeViewport.SCULPT_STATE_SIZE];
        NativeViewport.sculptState(state);
        return state;
    }

    private static long[] viewSceneIds() {
        final long[] ids = new long[16];
        final int n = NativeViewport.debugViewSceneBodyIds(ids);
        final long[] out = new long[n];
        System.arraycopy(ids, 0, out, 0, n);
        return out;
    }

    private void setCameraPose(final float yaw, final float pitch, final float distance) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTrue("the camera pose is accepted",
                    NativeViewport.debugSetCameraPose(yaw, pitch, distance));
            return null;
        });
        settleLayout();
    }

    private static float[] cameraPose() {
        final float[] pose = new float[NativeViewport.CAMERA_POSE_SIZE];
        NativeViewport.debugCameraPose(pose);
        return pose;
    }

    private static float[] projected(double x, double y, double z) {
        final float[] out = new float[2];
        assertTrue("the point projects onto the viewport",
                NativeViewport.debugProjectWorld(x, y, z, out));
        return out;
    }

    /**
     * A viewport pixel no body can be under. The two bodies stand within
     * three metres of the orbit target, framed at the viewport's middle from
     * eight metres away, so the top strip of the surface looks past both of
     * them at nothing. Measured in the surface's own pixels, the space
     * `debugProjectWorld` answers in.
     */
    private float[] emptyPixel() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            return new float[] {viewport.getWidth() * 0.5f, viewport.getHeight() * 0.06f};
        });
    }

    /** One real, non-stroke tap on the viewport surface. */
    private void tapViewport(final float x, final float y) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final long down = SystemClock.uptimeMillis();
            send(viewport, down, down, MotionEvent.ACTION_DOWN, x, y);
            send(viewport, down, down + 40L, MotionEvent.ACTION_UP, x, y);
            return null;
        });
        settleLayout();
    }

    /** One real single-finger drag on the viewport surface. */
    private void dragViewport(final float x0, final float y0, final float x1, final float y1) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final long down = SystemClock.uptimeMillis();
            send(viewport, down, down, MotionEvent.ACTION_DOWN, x0, y0);
            for (int step = 1; step <= 10; ++step) {
                final float t = step / 10f;
                send(viewport, down, down + step * 12L, MotionEvent.ACTION_MOVE,
                        x0 + (x1 - x0) * t, y0 + (y1 - y0) * t);
            }
            send(viewport, down, down + 132L, MotionEvent.ACTION_UP, x1, y1);
            return null;
        });
        settleLayout();
    }

    private static void send(View target, long downTime, long eventTime, int action,
                             float x, float y) {
        final MotionEvent event = MotionEvent.obtain(downTime, eventTime, action, x, y, 0);
        try {
            target.dispatchTouchEvent(event);
        } finally {
            event.recycle();
        }
    }

    // --- captured-frame measurement -------------------------------------------

    /** Half the side of every sampled square, in BITMAP pixels: 24 x 24 = 576. */
    private static final int kSampleHalf = 12;
    /** Per-pixel change tolerance: the summed |dR| + |dG| + |dB|. */
    private static final int kPixelTolerance = 24;
    /** A body in its square differs from the empty ground by at least this mean distance. */
    private static final int kBodySignal = 2 * kPixelTolerance;
    /** Allowed changed fraction above the measured noise floor for "unchanged". */
    private static final double kStableFraction = 0.05;
    /** A square whose body left must have changed at least this much. */
    private static final double kGoneFraction = 0.90;
    /** ...and must now sit at least this many times closer to the empty ground. */
    private static final int kGoneContrastRatio = 4;

    /**
     * Everything needed to turn a viewport pixel into a captured-bitmap pixel,
     * read once from the live views: the surface's size and screen origin, and
     * the display's real size in the same logical screen space
     * {@code getLocationOnScreen} answers in.
     */
    private CaptureGeometry captureGeometry(final float[] atA, final float[] atB,
                                            final float[] atEmpty) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final int[] origin = new int[2];
            viewport.getLocationOnScreen(origin);
            final int displayW;
            final int displayH;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                final Rect bounds =
                        activity.getWindowManager().getMaximumWindowMetrics().getBounds();
                displayW = bounds.width();
                displayH = bounds.height();
            } else {
                final Point size = new Point();
                activity.getWindowManager().getDefaultDisplay().getRealSize(size);
                displayW = size.x;
                displayH = size.y;
            }
            return new CaptureGeometry(viewport.getWidth(), viewport.getHeight(), origin[0],
                    origin[1], displayW, displayH, atA, atB, atEmpty);
        });
    }

    /**
     * The explicit conversion from the surface's own pixels to the captured
     * bitmap's, and the sampling that refuses anything it cannot measure in
     * full. The bitmap's size is never assumed: it is read from each capture,
     * and the screen-to-bitmap scale is bitmap size over display size per
     * axis, so a scaled or letterboxed capture is converted rather than
     * misread. A square that is not wholly inside the bitmap FAILS, with every
     * number that went into it -- it is never clipped to what happens to fit.
     */
    static final class CaptureGeometry {
        final int viewW;
        final int viewH;
        final int viewLeft;
        final int viewTop;
        final int displayW;
        final int displayH;
        final float[] a;
        final float[] b;
        final float[] empty;

        CaptureGeometry(int viewW, int viewH, int viewLeft, int viewTop, int displayW,
                        int displayH, float[] a, float[] b, float[] empty) {
            this.viewW = viewW;
            this.viewH = viewH;
            this.viewLeft = viewLeft;
            this.viewTop = viewTop;
            this.displayW = displayW;
            this.displayH = displayH;
            this.a = a;
            this.b = b;
            this.empty = empty;
        }

        /**
         * The square around a viewport pixel, as {left, top, right, bottom} in
         * the bitmap's pixels (right and bottom exclusive). Pure arithmetic.
         */
        static int[] bitmapSquare(int bitmapW, int bitmapH, int displayW, int displayH,
                                  int viewLeft, int viewTop, float[] viewportPixel, int half) {
            final double scaleX = (double) bitmapW / displayW;
            final double scaleY = (double) bitmapH / displayH;
            final int cx = (int) Math.round((viewLeft + viewportPixel[0]) * scaleX);
            final int cy = (int) Math.round((viewTop + viewportPixel[1]) * scaleY);
            return new int[] {cx - half, cy - half, cx + half, cy + half};
        }

        int[] square(Bitmap bitmap, float[] viewportPixel) {
            return bitmapSquare(bitmap.getWidth(), bitmap.getHeight(), displayW, displayH,
                    viewLeft, viewTop, viewportPixel, kSampleHalf);
        }

        String describe(Bitmap bitmap) {
            return "bitmap=" + bitmap.getWidth() + "x" + bitmap.getHeight()
                    + " config=" + bitmap.getConfig()
                    + " display=" + displayW + "x" + displayH
                    + " scale=" + ((double) bitmap.getWidth() / displayW) + "/"
                    + ((double) bitmap.getHeight() / displayH)
                    + " viewport=" + viewW + "x" + viewH + "@" + viewLeft + "," + viewTop
                    + " projA=" + point(a) + " projB=" + point(b)
                    + " projEmpty=" + point(empty)
                    + " rectA=" + rect(square(bitmap, a)) + " rectB=" + rect(square(bitmap, b))
                    + " rectEmpty=" + rect(square(bitmap, empty))
                    + " pixelsPerRect=" + (4 * kSampleHalf * kSampleHalf);
        }

        /** Fails loudly unless the whole square is inside both the viewport and the bitmap. */
        int[] checkedSquare(Bitmap bitmap, float[] viewportPixel, String what) {
            final String where = what + " [" + describe(bitmap) + "]";
            assertTrue("the capture has pixels: " + where,
                    bitmap.getWidth() > 0 && bitmap.getHeight() > 0);
            assertTrue("the display has a size: " + where, displayW > 0 && displayH > 0);
            assertTrue("the projected point is finite and inside the viewport: " + where,
                    Float.isFinite(viewportPixel[0]) && Float.isFinite(viewportPixel[1])
                            && viewportPixel[0] - kSampleHalf >= 0
                            && viewportPixel[1] - kSampleHalf >= 0
                            && viewportPixel[0] + kSampleHalf <= viewW
                            && viewportPixel[1] + kSampleHalf <= viewH);
            final int[] r = square(bitmap, viewportPixel);
            final int count = (r[2] - r[0]) * (r[3] - r[1]);
            assertTrue("the square is non-empty and wholly inside the bitmap (" + count
                            + " pixels): " + where,
                    r[2] > r[0] && r[3] > r[1] && r[0] >= 0 && r[1] >= 0
                            && r[2] <= bitmap.getWidth() && r[3] <= bitmap.getHeight());
            return r;
        }

        Sample sample(Bitmap bitmap, float[] viewportPixel, String what) {
            final int[] r = checkedSquare(bitmap, viewportPixel, what);
            long red = 0;
            long green = 0;
            long blue = 0;
            int n = 0;
            for (int y = r[1]; y < r[3]; ++y) {
                for (int x = r[0]; x < r[2]; ++x) {
                    final int p = bitmap.getPixel(x, y);
                    red += Color.red(p);
                    green += Color.green(p);
                    blue += Color.blue(p);
                    n++;
                }
            }
            assertEquals("every pixel of the square was measured: " + what,
                    4 * kSampleHalf * kSampleHalf, n);
            return new Sample((int) (red / n), (int) (green / n), (int) (blue / n));
        }

        /** Fraction of the square's pixels that changed by more than the tolerance. */
        double changedFraction(Bitmap before, Bitmap after, float[] viewportPixel) {
            assertEquals("both captures have one size", before.getWidth(), after.getWidth());
            assertEquals("both captures have one size", before.getHeight(), after.getHeight());
            final int[] r = checkedSquare(before, viewportPixel, "changed");
            int changed = 0;
            int total = 0;
            for (int y = r[1]; y < r[3]; ++y) {
                for (int x = r[0]; x < r[2]; ++x) {
                    final int p = before.getPixel(x, y);
                    final int q = after.getPixel(x, y);
                    final int d = Math.abs(Color.red(p) - Color.red(q))
                            + Math.abs(Color.green(p) - Color.green(q))
                            + Math.abs(Color.blue(p) - Color.blue(q));
                    if (d > kPixelTolerance) {
                        changed++;
                    }
                    total++;
                }
            }
            assertEquals("every pixel of the square was compared",
                    4 * kSampleHalf * kSampleHalf, total);
            return (double) changed / total;
        }

        private static String point(float[] p) {
            return "(" + p[0] + "," + p[1] + ")";
        }

        private static String rect(int[] r) {
            return "[" + r[0] + "," + r[1] + "-" + r[2] + "," + r[3] + "]";
        }
    }

    /** One square's mean colour. */
    static final class Sample {
        final int red;
        final int green;
        final int blue;

        Sample(int red, int green, int blue) {
            this.red = red;
            this.green = green;
            this.blue = blue;
        }

        /** Summed per-channel distance between two means. */
        int distanceTo(Sample other) {
            return Math.abs(red - other.red) + Math.abs(green - other.green)
                    + Math.abs(blue - other.blue);
        }

        @Override
        public String toString() {
            return red + "/" + green + "/" + blue;
        }
    }

    /**
     * The composed display, Vulkan viewport included, after frames settle, as
     * a software bitmap whose pixels can be read.
     */
    private static Bitmap capture() {
        SystemClock.sleep(600);
        final Bitmap shot =
                InstrumentationRegistry.getInstrumentation().getUiAutomation().takeScreenshot();
        assertNotNull("the display can be captured", shot);
        if (shot.getConfig() == Bitmap.Config.ARGB_8888) {
            return shot;
        }
        final Bitmap readable = shot.copy(Bitmap.Config.ARGB_8888, false);
        assertNotNull("the capture converts to a readable bitmap", readable);
        return readable;
    }

    private static void assertTransform(int status) {
        assertTrue("the placement lands: " + status, status == NativeViewport.APPLY_APPLIED
                || status == NativeViewport.APPLY_UNCHANGED);
    }
}
