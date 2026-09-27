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

        final byte[] bytesBefore = NativeViewport.encodeProject();
        final long fingerprintBefore = NativeViewport.projectFingerprint();
        final int sculptUndoBefore = NativeViewport.sculptUndoDepth();
        final double revisionBefore = sculptState()[NativeViewport.SCULPT_REVISION];

        // The frame before: B is drawn at its projected centre.
        final int[] regionB = region(projected(2.5, 0.0, 0.0));
        final int[] regionA = region(projected(0.0, 0.0, 0.0));
        final Bitmap off = captureBareViewport();

        // Probe the seam before trusting it: does a captured frame carry the
        // Vulkan viewport on THIS device at all? Turning the camera moves B
        // off its pixel, so a capture that sees the viewport must change
        // there. (A composed-display capture that omits the SurfaceView layer
        // would otherwise report "nothing changed" for every assertion below
        // and prove nothing either way.)
        final float[] pose = cameraPose();
        setCameraPose(pose[NativeViewport.CAMERA_POSE_YAW] + 1.2f,
                pose[NativeViewport.CAMERA_POSE_PITCH], pose[NativeViewport.CAMERA_POSE_DISTANCE]);
        final Bitmap turned = captureBareViewport();
        setCameraPose(pose[NativeViewport.CAMERA_POSE_YAW], pose[NativeViewport.CAMERA_POSE_PITCH],
                pose[NativeViewport.CAMERA_POSE_DISTANCE]);
        final double probe = changedFraction(off, turned, regionB);
        final boolean captureSeesViewport = probe > 0.2;
        Log.i(TAG, "STAGE027_CAPTURE_PROBE changedAtBWhenTurned=" + probe + " meanOffB="
                + meanRgb(off, regionB) + " meanTurnedB=" + meanRgb(turned, regionB)
                + " seesViewport=" + captureSeesViewport);

        clickIsolate();
        assertTrue("native reports the viewport isolated", NativeViewport.sculptIsolated());
        assertIsolateControl(true);
        assertArrayEquals("isolated, the viewport's list is the Sculpt target alone",
                new long[] {bodyA}, viewSceneIds());

        final Bitmap on = captureBareViewport();
        final double changedAtB = changedFraction(off, on, regionB);
        final double changedAtA = changedFraction(off, on, regionA);
        Log.i(TAG, "STAGE027_ISOLATE_FRAME changedAtB=" + changedAtB + " changedAtA=" + changedAtA
                + " meanOffB=" + meanRgb(off, regionB) + " meanOnB=" + meanRgb(on, regionB)
                + " seesViewport=" + captureSeesViewport);
        if (captureSeesViewport) {
            assertTrue("B is no longer drawn where it stood: " + changedAtB, changedAtB > 0.5);
            assertTrue("A is still drawn where it stood: " + changedAtA, changedAtA < 0.2);
        } else {
            // Stated, never silently passed: on a device whose display capture
            // does not carry the Vulkan layer, the evidence is the exact list
            // the renderer is handed, asserted above as {A}.
            Log.i(TAG, "STAGE027_CAPTURE_SEAM_UNAVAILABLE the captured frame did not change when "
                    + "the camera turned (" + probe + "); the viewport-list assertion stands");
        }

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

    private static String meanRgb(Bitmap bitmap, int[] r) {
        long red = 0;
        long green = 0;
        long blue = 0;
        int n = 0;
        for (int y = Math.max(0, r[1]); y < Math.min(bitmap.getHeight(), r[3]); ++y) {
            for (int x = Math.max(0, r[0]); x < Math.min(bitmap.getWidth(), r[2]); ++x) {
                final int p = bitmap.getPixel(x, y);
                red += Color.red(p);
                green += Color.green(p);
                blue += Color.blue(p);
                n++;
            }
        }
        return n == 0 ? "none" : (red / n) + "/" + (green / n) + "/" + (blue / n);
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

    /** A small square around a viewport pixel, in SCREEN pixels. */
    private int[] region(final float[] viewportPixel) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final int[] at = new int[2];
            workspace.findViewById(R.id.viewport_surface).getLocationOnScreen(at);
            final int half = 16;
            final int cx = at[0] + Math.round(viewportPixel[0]);
            final int cy = at[1] + Math.round(viewportPixel[1]);
            return new int[] {cx - half, cy - half, cx + half, cy + half};
        });
    }

    /** The composed display, Vulkan viewport included, after frames settle. */
    private static Bitmap capture() {
        SystemClock.sleep(600);
        final Bitmap shot =
                InstrumentationRegistry.getInstrumentation().getUiAutomation().takeScreenshot();
        assertNotNull("the display can be captured", shot);
        return shot;
    }

    /** Fraction of the region's pixels that changed by more than a small tolerance. */
    private static double changedFraction(Bitmap a, Bitmap b, int[] r) {
        int changed = 0;
        int total = 0;
        for (int y = Math.max(0, r[1]); y < Math.min(a.getHeight(), r[3]); ++y) {
            for (int x = Math.max(0, r[0]); x < Math.min(a.getWidth(), r[2]); ++x) {
                final int p = a.getPixel(x, y);
                final int q = b.getPixel(x, y);
                final int d = Math.abs(Color.red(p) - Color.red(q))
                        + Math.abs(Color.green(p) - Color.green(q))
                        + Math.abs(Color.blue(p) - Color.blue(q));
                if (d > 24) {
                    changed++;
                }
                total++;
            }
        }
        return total == 0 ? 0.0 : (double) changed / total;
    }

    private static void assertTransform(int status) {
        assertTrue("the placement lands: " + status, status == NativeViewport.APPLY_APPLIED
                || status == NativeViewport.APPLY_UNCHANGED);
    }
}
