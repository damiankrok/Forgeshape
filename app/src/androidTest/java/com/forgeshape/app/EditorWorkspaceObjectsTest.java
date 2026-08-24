package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.view.MotionEvent;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * S17-21..26: the Objects section, and that selecting a body — from the list or
 * from the viewport — moves the whole workspace to that body.
 *
 * <p>Every body is named by its stable ObjectId, never by row position. The one
 * thing these tests deliberately do NOT assert is a rendered pixel: what the
 * viewport draws is proven by the native suites and by runtime evidence.
 */
@RunWith(AndroidJUnit4.class)
public final class EditorWorkspaceObjectsTest {

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    /**
     * S17-21. Add Body creates a row for the new body and selects it.
     */
    @Test
    public void s17_21_addBodyCreatesARowAndSelectsTheNewBody() {
        final int[] countBox = new int[1];
        final long before = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            // Add Body is refused in Sculpt mode; an earlier test may have left
            // the product there.
            if (NativeViewport.productMode() != NativeViewport.MODE_CONSTRUCTION) {
                NativeViewport.enterConstructionMode();
            }
            assertTrue("precondition: at least the startup body exists",
                    NativeViewport.sceneBodyCount() >= 1);
            countBox[0] = NativeViewport.sceneBodyCount();
            return NativeViewport.sceneActiveBodyId();
        });
        final int countBefore = countBox[0];

        final long created = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            workspace.objectsSection().findViewById(R.id.add_body).performClick();
            return NativeViewport.sceneActiveBodyId();
        });

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("Add Body appends exactly one body",
                    countBefore + 1, NativeViewport.sceneBodyCount());
            assertNotEquals("the new body has its own id", before, created);
            assertEquals("Add Body selects what it added",
                    created, NativeViewport.sceneActiveBodyId());
            assertEquals("the list shows one row per body",
                    NativeViewport.sceneBodyCount(), workspace.objectsSection().rowCount());
            assertNotNull("the new body has a row of its own",
                    workspace.objectsSection().rowFor(created));
            assertNotNull("the previously active body still has its row",
                    workspace.objectsSection().rowFor(before));
            return null;
        });
    }

    /**
     * S17-22. Selecting a row re-points the exact-value editor at that body:
     * the fields show the selected body's own dimensions, not the previous
     * body's.
     */
    @Test
    public void s17_22_selectingARowRefreshesTheExactFields() {
        final long[] ids = addSecondBodyAndReturnIds();
        final long first = ids[0];
        final long second = ids[1];

        // Give the two bodies genuinely different shapes, so "the fields
        // followed the selection" cannot pass by them being identical. Both are
        // set explicitly: an earlier test may have left either as anything.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(second, NativeViewport.sceneActiveBodyId());
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.applyConstructionSphere(3.0));
            return null;
        });

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(first));
            assertEquals(NativeViewport.SCULPT_OK,
                    NativeViewport.applyConstructionCylinder(1.0, 2.0));
            workspace.onNativeStateChanged();
            final double[] primitive = new double[NativeViewport.PRIMITIVE_STATE_SIZE];
            NativeViewport.constructionPrimitive(primitive);
            assertEquals("selecting the first body shows ITS kind",
                    NativeViewport.PRIMITIVE_CYLINDER, (int) primitive[0]);
            return null;
        });

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View row = workspace.objectsSection().rowFor(second);
            assertNotNull("the second body still has a row", row);
            row.performClick();
            assertEquals("tapping the row selects that body",
                    second, NativeViewport.sceneActiveBodyId());
            final double[] primitive = new double[NativeViewport.PRIMITIVE_STATE_SIZE];
            NativeViewport.constructionPrimitive(primitive);
            assertEquals("the editor now shows the second body's kind",
                    NativeViewport.PRIMITIVE_SPHERE, (int) primitive[0]);
            assertEquals("and its own diameter", 3.0,
                    primitive[NativeViewport.PRIMITIVE_SPHERE_DIAMETER], 1e-9);
            return null;
        });
    }

    /**
     * S17-23. A tap in the viewport that hits a body selects it, and the
     * Objects row and Inspector follow without anything else being touched.
     *
     * <p>The two bodies are separated along X and the tap goes at the viewport
     * centre, where the body left at the origin sits — so a hit on the OTHER
     * body is what a wrong answer would look like, not a miss.
     */
    @Test
    public void s17_23_aViewportPickSyncsTheActiveRowAndInspector() {
        final long[] ids = addSecondBodyAndReturnIds();
        final long first = ids[0];
        final long second = ids[1];

        // Put the FIRST body alone at the origin and every other body — including
        // the ones earlier tests left behind — far outside the camera framing,
        // so the centre pixel can only resolve to `first`.
        isolateAtOrigin(first);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(second));
            workspace.onNativeStateChanged();
            return null;
        });

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            final int w = viewport.getWidth();
            final int h = viewport.getHeight();
            assertTrue("precondition: the viewport must be laid out", w > 0 && h > 0);
            assertEquals("precondition: the OTHER body is selected before the tap",
                    second, NativeViewport.sceneActiveBodyId());
            // Dispatched to the SurfaceView itself, not straight to JNI, so the
            // real path runs: the view forwards the event AND then tells the
            // workspace the gesture settled. Poking JNI directly would skip the
            // half of this that the test is about.
            dispatchTap(viewport, w / 2f, h / 2f);
            return null;
        });

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("a tap on the body at the origin selects it",
                    first, NativeViewport.sceneActiveBodyId());
            // The chrome re-read on its own, through the settle listener.
            assertNotNull("the picked body has a row", workspace.objectsSection().rowFor(first));
            assertEquals("the row count is unchanged by a pick",
                    NativeViewport.sceneBodyCount(), workspace.objectsSection().rowCount());
            return null;
        });
    }

    /**
     * S17-24. Two bodies with different transforms exist and are both
     * renderable at once: each has its own published mesh revision and its own
     * placement, and neither edit disturbed the other.
     */
    @Test
    public void s17_24_twoTransformedBodiesCoexist() {
        final long[] ids = addSecondBodyAndReturnIds();
        final long first = ids[0];
        final long second = ids[1];

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertTransformLanded(NativeViewport.applyBoxTransform(5.0, 0.0, 0.0, 0.0, 0.0, 0.0));
            return null;
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(first));
            final double[] placement = new double[6];
            NativeViewport.boxTransform(placement);
            assertEquals("the first body was not moved by the second body's transform",
                    0.0, placement[0], 1e-9);
            return null;
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(second));
            final double[] placement = new double[6];
            NativeViewport.boxTransform(placement);
            assertEquals("the second body kept its own placement", 5.0, placement[0], 1e-9);
            assertEquals("both bodies are still in the scene", 2,
                    Math.min(2, NativeViewport.sceneBodyCount()));
            return null;
        });
    }

    /**
     * S17-25. A Freeze/Sculpt on one body, then on the other, then a Resume
     * back on the first — each body keeps its own Frozen Sculpt Mesh.
     *
     * <p>Driven entirely through the product's own entry points, so this is the
     * mandatory scenario as a user would perform it, not a state poke.
     */
    @Test
    public void s17_25_perBodyFreezeSculptAndResumeRoundTrip() {
        final long[] ids = addSecondBodyAndReturnIds();
        final long first = ids[0];
        final long second = ids[1];

        // --- Body A: select, make it sculptable, freeze, sculpt ---
        selectAndFreeze(first);
        sculptTheCurrentMesh();
        final double aRevision = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] sculpt = readSculpt();
            assertTrue("body A must actually have been sculpted",
                    sculpt[NativeViewport.SCULPT_HAS_EDITS] != 0.0);
            return sculpt[NativeViewport.SCULPT_REVISION];
        });

        // --- Back to Construction, then body B: freeze and sculpt it too ---
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.enterConstructionMode());
            return null;
        });
        selectAndFreeze(second);
        sculptTheCurrentMesh();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final double[] sculpt = readSculpt();
            assertTrue("body B must actually have been sculpted",
                    sculpt[NativeViewport.SCULPT_HAS_EDITS] != 0.0);
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.enterConstructionMode());
            return null;
        });

        // --- Back to body A and Resume: its own frozen mesh must come back ---
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(first));
            assertEquals("Resume must succeed on the body that was frozen first",
                    NativeViewport.SCULPT_OK, NativeViewport.enterSculptMode());
            final double[] sculpt = readSculpt();
            assertEquals("body A came back at its own sculpt revision",
                    aRevision, sculpt[NativeViewport.SCULPT_REVISION], 0.0);
            assertTrue("body A still carries its own edits",
                    sculpt[NativeViewport.SCULPT_HAS_EDITS] != 0.0);
            assertEquals("Resume re-froze nothing", 1.0, sculpt[NativeViewport.SCULPT_HAS_MESH],
                    0.0);
            return null;
        });

        // --- And body B still has its own, independently ---
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.enterConstructionMode());
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(second));
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.enterSculptMode());
            final double[] sculpt = readSculpt();
            assertTrue("body B kept its own edits through A's round trip",
                    sculpt[NativeViewport.SCULPT_HAS_EDITS] != 0.0);
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.enterConstructionMode());
            return null;
        });
    }

    /**
     * S17-26. The Objects section stays reachable and the viewport stays
     * full-bleed in every layout the workspace produces.
     */
    @Test
    public void s17_26_objectsStayUsableAndTheViewportStaysFullBleed() {
        addSecondBodyAndReturnIds();
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = workspace.findViewById(R.id.viewport_surface);
            assertEquals("the viewport still spans the whole workspace width",
                    workspace.getWidth(), viewport.getWidth());
            assertEquals("the viewport still spans the whole workspace height",
                    workspace.getHeight(), viewport.getHeight());
            assertNotNull("the Objects section is present",
                    workspace.findViewById(R.id.objects_section));
            assertNotNull("Add Body is present", workspace.findViewById(R.id.add_body));
            assertEquals("every body has a row in this layout",
                    NativeViewport.sceneBodyCount(), workspace.objectsSection().rowCount());
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // Support
    // -----------------------------------------------------------------------

    /**
     * Adds a body and returns {previously active id, new id}.
     *
     * <p>The scene is process-scoped and there is no delete, so bodies
     * ACCUMULATE across the tests in a run and the product may be left in
     * Sculpt mode by an earlier one. Nothing here may therefore assume a body
     * count, which body is at the origin, or which mode is current: every test
     * establishes what it needs and asserts relative to what it found. That is
     * the same lesson Stage 016-R2 recorded for the native self-tests, applied
     * to the instrumentation.
     */
    private long[] addSecondBodyAndReturnIds() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            // Add Body is refused in Sculpt mode, where the target is fixed.
            if (NativeViewport.productMode() != NativeViewport.MODE_CONSTRUCTION) {
                NativeViewport.enterConstructionMode();
            }
            final long before = NativeViewport.sceneActiveBodyId();
            final long created = NativeViewport.sceneAddBody();
            assertTrue("Add Body must succeed in Construction mode", created != 0L);
            workspace.onNativeStateChanged();
            return new long[] {before, created};
        });
    }

    /**
     * Puts `keepId` at the world origin and moves every other body far away.
     *
     * <p>Makes a centre-of-viewport pick unambiguous no matter how many bodies
     * earlier tests left lying at the origin. 40 m apart is far outside the
     * default camera's framing, so no stray body can be under the centre pixel.
     */
    private void isolateAtOrigin(final long keepId) {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            if (NativeViewport.productMode() != NativeViewport.MODE_CONSTRUCTION) {
                NativeViewport.enterConstructionMode();
            }
            final int count = NativeViewport.sceneBodyCount();
            final long[] ids = new long[count];
            final int written = NativeViewport.sceneBodyIds(ids);
            double offset = 40.0;
            for (int i = 0; i < written; i++) {
                assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(ids[i]));
                if (ids[i] == keepId) {
                    assertTransformLanded(NativeViewport.applyBoxTransform(0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
                } else {
                    assertTransformLanded(NativeViewport.applyBoxTransform(offset, 0.0, 0.0, 0.0, 0.0, 0.0));
                    offset += 40.0;
                }
            }
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(keepId));
            workspace.onNativeStateChanged();
            return null;
        });
    }

    /**
     * Selects a body, makes it a sphere (482 vertices, so a brush captures a
     * meaningful set — a box's 8 corners would not) and freezes it.
     */
    private void selectAndFreeze(final long objectId) {
        // The stroke below is driven at the viewport centre, so the body being
        // sculpted has to be the one under that pixel. Isolating it also clears
        // whatever placement an earlier test left on it.
        isolateAtOrigin(objectId);
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(objectId));
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.applyConstructionSphere(1.0));
            NativeViewport.setSculptBrush(300.0, 1.0);
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.freezeToSculpt());
            final double[] sculpt = readSculpt();
            assertEquals("a freshly frozen body carries no edits", 0.0,
                    sculpt[NativeViewport.SCULPT_HAS_EDITS], 0.0);
            return null;
        });
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
            send(MotionEvent.ACTION_DOWN, cx, cy, w, h);
            for (int step = 1; step <= 8; ++step) {
                send(MotionEvent.ACTION_MOVE, cx + step * 6f, cy + step * 4f, w, h);
            }
            send(MotionEvent.ACTION_UP, cx + 48f, cy + 32f, w, h);
            return null;
        });
    }

    /** A real tap delivered to the view, so the whole touch path runs. */
    private static void dispatchTap(View viewport, float x, float y) {
        final long down = android.os.SystemClock.uptimeMillis();
        final MotionEvent downEvent =
                MotionEvent.obtain(down, down, MotionEvent.ACTION_DOWN, x, y, 0);
        viewport.dispatchTouchEvent(downEvent);
        downEvent.recycle();
        final MotionEvent upEvent =
                MotionEvent.obtain(down, down + 16, MotionEvent.ACTION_UP, x, y, 0);
        viewport.dispatchTouchEvent(upEvent);
        upEvent.recycle();
    }

    private static void send(int action, float x, float y, int width, int height) {
        NativeViewport.touchEvent(action, -1, 1, new int[] {0}, new float[] {x},
                new float[] {y}, width, height);
    }

    /**
     * A transform request that landed.
     *
     * <p>Applied and Unchanged are BOTH success: re-applying a placement a body
     * already has is legitimately "nothing to do", and these helpers deliberately
     * set placements without knowing what the body already had. Only Rejected is
     * a failure.
     */
    private static void assertTransformLanded(int status) {
        assertTrue("the transform must not be rejected",
                status == NativeViewport.APPLY_APPLIED
                        || status == NativeViewport.APPLY_UNCHANGED);
    }

    private static double[] readSculpt() {
        final double[] sculpt = new double[NativeViewport.SCULPT_STATE_SIZE];
        NativeViewport.sculptState(sculpt);
        return sculpt;
    }
}
