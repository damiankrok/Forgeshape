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

import android.os.SystemClock;
import android.util.Log;
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
 * `STAGE027` PHASE 0 — runtime reproduction of the two audit findings on the
 * UNFIXED product, through the real product paths.
 *
 * <p>These cases assert that each defect IS present. They are evidence, not a
 * contract: a green run on a pre-fix build is the proof `STAGE027-R0` asked for
 * before any fix is written, and the fixed class replaces them.
 *
 * <ul>
 *   <li>{@code FINDING-A}: in Sculpt, a non-stroke viewport tap on another body
 *       moves the active body — and with it the Sculpt target — while the
 *       Objects-row path refuses the same act.</li>
 *   <li>{@code FINDING-B}: Start Sculpting and Resume Sculpt both enter Sculpt
 *       on a body the user has hidden.</li>
 * </ul>
 *
 * <p>Every act is driven the way a user drives it: real MotionEvents on the
 * viewport surface, the toolbar transitions and the Objects-row Show/Hide
 * control by their semantic ids. Native setters are used only to BUILD the
 * scene the journey starts from, never to perform the act under test.
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
        // A hidden or sculpting body must not become the next class's baseline.
        freshProject();
    }

    // -----------------------------------------------------------------------
    // FINDING-A
    // -----------------------------------------------------------------------

    @Test
    public void phase0_findingA_aSculptTapOnAnotherBodyMovesTheSculptTarget() {
        startSculptingThroughTheToolbar();
        final double[] before = sculptState();
        assertEquals("precondition: Sculpt is on", NativeViewport.MODE_SCULPT,
                (int) before[NativeViewport.SCULPT_MODE]);
        assertEquals("precondition: A is the Sculpt target", bodyA,
                (long) before[NativeViewport.SCULPT_OBJECT_ID]);
        assertEquals("precondition: A is active", bodyA, NativeViewport.sceneActiveBodyId());

        // The Objects-row path refuses the same act -- the documented rule.
        assertNotEquals("the row path refuses a body switch in Sculpt", NativeViewport.SCULPT_OK,
                NativeViewport.sceneSelectBody(bodyB));
        assertEquals(bodyA, NativeViewport.sceneActiveBodyId());

        final float[] onB = projected(2.5, 0.0, 0.0);
        final float[] onA = projected(0.0, 0.0, 0.0);
        assertTrue("B's centre is well clear of A's silhouette on screen",
                Math.hypot(onB[0] - onA[0], onB[1] - onA[1]) > 80.0);

        tapViewport(onB[0], onB[1]);

        final double[] after = sculptState();
        final long active = NativeViewport.sceneActiveBodyId();
        Log.i(TAG, "STAGE027_REPRO_FINDING_A tap_on_B active=" + active + " mode="
                + (int) after[NativeViewport.SCULPT_MODE] + " sculptObject="
                + (long) after[NativeViewport.SCULPT_OBJECT_ID] + " hasMesh="
                + (int) after[NativeViewport.SCULPT_HAS_MESH] + " viewportSelection="
                + NativeViewport.debugViewportSelection() + " A=" + bodyA + " B=" + bodyB);
        // THE DEFECT: the viewport path bypassed the row guard.
        assertEquals("FINDING-A reproduced: the tap made B active", bodyB, active);
        assertEquals("while Sculpt is still on", NativeViewport.MODE_SCULPT,
                (int) after[NativeViewport.SCULPT_MODE]);
        assertNotEquals("and the Sculpt target is no longer A", bodyA,
                (long) after[NativeViewport.SCULPT_OBJECT_ID]);
        assertEquals("and the viewport selection is B", bodyB,
                NativeViewport.debugViewportSelection());

        // A non-stroke tap that misses everything clears the selection.
        final float[] empty = emptyPixel();
        tapViewport(empty[0], empty[1]);
        final long selectionAfterMiss = NativeViewport.debugViewportSelection();
        Log.i(TAG, "STAGE027_REPRO_FINDING_A miss viewportSelection=" + selectionAfterMiss
                + " active=" + NativeViewport.sceneActiveBodyId());
        assertEquals("a Sculpt miss clears the viewport selection", 0L, selectionAfterMiss);
    }

    // -----------------------------------------------------------------------
    // FINDING-B
    // -----------------------------------------------------------------------

    @Test
    public void phase0_findingB_startSculptingEntersSculptOnAHiddenBody() {
        hideThroughTheObjectsRow(bodyA);
        assertFalse("precondition: A is hidden", NativeViewport.sceneBodyVisible(bodyA));
        assertEquals("precondition: A is still active", bodyA, NativeViewport.sceneActiveBodyId());
        assertEquals("precondition: nothing frozen yet", 0.0,
                sculptState()[NativeViewport.SCULPT_HAS_MESH], 0.0);

        final boolean offered = isShown(R.id.freeze_to_sculpt);
        clickToolbar(R.id.freeze_to_sculpt);

        final double[] after = sculptState();
        Log.i(TAG, "STAGE027_REPRO_FINDING_B start offered=" + offered + " mode="
                + (int) after[NativeViewport.SCULPT_MODE] + " hasMesh="
                + (int) after[NativeViewport.SCULPT_HAS_MESH] + " visible="
                + NativeViewport.sceneBodyVisible(bodyA));
        assertTrue("Start Sculpting is offered over the hidden body", offered);
        assertEquals("FINDING-B reproduced: Sculpt was entered", NativeViewport.MODE_SCULPT,
                (int) after[NativeViewport.SCULPT_MODE]);
        assertEquals("a Frozen Sculpt Mesh now exists for the hidden body", 1.0,
                after[NativeViewport.SCULPT_HAS_MESH], 0.0);
        assertEquals(bodyA, (long) after[NativeViewport.SCULPT_OBJECT_ID]);
        assertFalse("while the body is still hidden", NativeViewport.sceneBodyVisible(bodyA));
    }

    @Test
    public void phase0_findingB_resumeSculptEntersSculptOnAHiddenBody() {
        startSculptingThroughTheToolbar();
        clickToolbar(R.id.back_to_construction);
        assertEquals("precondition: back in Construction", NativeViewport.MODE_CONSTRUCTION,
                NativeViewport.productMode());
        hideThroughTheObjectsRow(bodyA);
        assertFalse("precondition: A is hidden", NativeViewport.sceneBodyVisible(bodyA));

        final boolean offered = isShown(R.id.resume_sculpt);
        clickToolbar(R.id.resume_sculpt);

        final double[] after = sculptState();
        Log.i(TAG, "STAGE027_REPRO_FINDING_B resume offered=" + offered + " mode="
                + (int) after[NativeViewport.SCULPT_MODE] + " visible="
                + NativeViewport.sceneBodyVisible(bodyA));
        assertTrue("Resume Sculpt is offered over the hidden body", offered);
        assertEquals("FINDING-B reproduced: Sculpt was resumed", NativeViewport.MODE_SCULPT,
                (int) after[NativeViewport.SCULPT_MODE]);
        assertFalse("while the body is still hidden", NativeViewport.sceneBodyVisible(bodyA));
    }

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

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
            control.performClick();
            return null;
        });
        settleLayout();
    }

    private void hideThroughTheObjectsRow(final long objectId) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            WorkspaceTestSupport.openObjectsPanel(workspace);
            return null;
        });
        settleLayout();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View more = find(workspace, R.id.object_row_more, objectId);
            assertNotNull("the row offers its overflow", more);
            more.performClick();
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

    private static void send(View target, long downTime, long eventTime, int action,
                             float x, float y) {
        final MotionEvent event = MotionEvent.obtain(downTime, eventTime, action, x, y, 0);
        try {
            target.dispatchTouchEvent(event);
        } finally {
            event.recycle();
        }
    }

    private static void assertTransform(int status) {
        assertTrue("the placement lands: " + status, status == NativeViewport.APPLY_APPLIED
                || status == NativeViewport.APPLY_UNCHANGED);
    }
}
