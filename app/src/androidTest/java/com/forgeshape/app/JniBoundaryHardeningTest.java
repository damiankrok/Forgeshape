package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.BASELINE_DEPTH_METERS;
import static com.forgeshape.app.WorkspaceTestSupport.BASELINE_HEIGHT_METERS;
import static com.forgeshape.app.WorkspaceTestSupport.BASELINE_WIDTH_METERS;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.view.MotionEvent;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicLong;

/**
 * `PAH-R1-03..06` and `PAH-R1-12..13`: the JNI boundary's two hardening rules
 * from the deep audit — a sculpt-reading entry point never rebinds the borrowed
 * sculpt target outside the state lock (F-08), and a touch event never makes a
 * JNI call while an array read has left an exception pending (F-11).
 *
 * <h2>How the lock rule is observed</h2>
 *
 * <p>{@code sculptSession()} re-points the one editing session at the ACTIVE
 * body's Frozen Sculpt Mesh on every call. The twelve values {@code sculptState}
 * reports therefore describe one body only if the whole read happens under the
 * same lock the body switch, the load and the history step take. A reader
 * thread — the same shape as the autosave worker, which reads the session under
 * the lock — hammers every audited reader while the UI thread switches, loads,
 * deletes and undoes, and every read must be INTERNALLY CONSISTENT: a report
 * that says "has a sculpt mesh" must carry that mesh's own vertex count and its
 * own body id, never a neighbour's. A torn read is the only way the invariant
 * can fail, and the lock is the only thing that prevents one.
 *
 * <p>Every concurrent case is bounded (a fixed number of mutations, a capped
 * reader loop, a join with a timeout), so a deadlock is a failed assertion and
 * never a hung suite.
 */
@RunWith(AndroidJUnit4.class)
public final class JniBoundaryHardeningTest {

    /** The second body's distinct box, so a cross-body primitive read is visible. */
    private static final double OTHER_WIDTH_METERS = 0.31;
    private static final double OTHER_HEIGHT_METERS = 0.47;
    private static final double OTHER_DEPTH_METERS = 0.59;

    /** Upper bound on reader iterations; the mutator's finish normally stops it first. */
    private static final int MAX_READER_ITERATIONS = 200_000;

    /** A reader that has not finished this long after the mutator did is stuck. */
    private static final long JOIN_TIMEOUT_MS = 30_000L;

    private static final int VIEW_WIDTH = 1000;
    private static final int VIEW_HEIGHT = 1000;

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    private byte[] baselineProject;

    @Before
    public void setUp() {
        resetToBaselineConstruction(rule.getScenario());
        // The invariants below name ONE frozen body and ONE other body, so the
        // scene must be exactly that. A class that ran earlier in the same
        // process may have left extra bodies or a retained sculpt mesh on body
        // 1 behind, and the ordinary reset keeps both (it re-applies a shape;
        // it does not close the project). Start from a fresh one-body project
        // instead, the way the reset itself does for a CAD-only scene: closing
        // writes nothing, and the seeded body is a plain Construction Body.
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.sketchCancel();
            NativeViewport.supportChooserCancel();
            NativeViewport.closeProject();
            workspace.ensureConstructionProjectForTest();
            return null;
        });
        resetToBaselineConstruction(rule.getScenario());
        baselineProject = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals("precondition: a fresh one-body project", 1,
                    NativeViewport.sceneBodyCount());
            assertEquals("precondition: no retained sculpt mesh on the seeded body", 0.0,
                    sculptState()[NativeViewport.SCULPT_HAS_MESH], 0.0);
            return NativeViewport.encodeProject();
        });
    }

    @After
    public void tearDown() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.enterConstructionMode();
            if (baselineProject != null) {
                NativeViewport.loadProject(baselineProject);
            }
            return null;
        });
        resetToBaselineConstruction(rule.getScenario());
    }

    // -----------------------------------------------------------------------
    // The two-body fixture
    // -----------------------------------------------------------------------

    /** Body A carries a Frozen Sculpt Mesh; body B is a plain Construction Body. */
    private static final class Fixture {
        long a;
        long b;
        int vertexCountA;
        int indexCountA;
        int tool;
    }

    private Fixture twoBodies() {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final Fixture f = new Fixture();
            f.a = NativeViewport.sceneActiveBodyId();
            assertNotEquals("precondition: a project with an active body", 0L, f.a);
            f.b = NativeViewport.sceneAddBody();
            assertNotEquals("a second body must be created", 0L, f.b);
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(f.b));
            assertEquals(NativeViewport.APPLY_APPLIED, NativeViewport.applyConstructionBox(
                    OTHER_WIDTH_METERS, OTHER_HEIGHT_METERS, OTHER_DEPTH_METERS));
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(f.a));
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.freezeToSculpt());
            final double[] state = sculptState();
            assertEquals(1.0, state[NativeViewport.SCULPT_HAS_MESH], 0.0);
            assertEquals((double) f.a, state[NativeViewport.SCULPT_OBJECT_ID], 0.0);
            f.vertexCountA = (int) state[NativeViewport.SCULPT_VERTEX_COUNT];
            f.indexCountA = (int) state[NativeViewport.SCULPT_INDEX_COUNT];
            assertTrue("the frozen mesh must have geometry", f.vertexCountA > 0);
            // Body switching, Delete and Construction Undo are refused while
            // sculpting, so the mutators below run in Construction mode; the
            // frozen mesh stays with body A regardless of the mode.
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.enterConstructionMode());
            f.tool = NativeViewport.sculptTool();
            return f;
        });
    }

    private static double[] sculptState() {
        final double[] state = new double[NativeViewport.SCULPT_STATE_SIZE];
        NativeViewport.sculptState(state);
        return state;
    }

    private static double[] primitiveState() {
        final double[] state = new double[NativeViewport.PRIMITIVE_STATE_SIZE];
        NativeViewport.constructionPrimitive(state);
        return state;
    }

    private static boolean isBox(double[] p, double w, double h, double d) {
        return Math.abs(p[1] - w) < 1e-9 && Math.abs(p[2] - h) < 1e-9 && Math.abs(p[3] - d) < 1e-9;
    }

    // -----------------------------------------------------------------------
    // The reader probe
    // -----------------------------------------------------------------------

    /**
     * Calls every audited sculpt-reading entry point in a loop from a non-UI
     * thread and records the first read that is not internally consistent.
     */
    private static final class ReaderProbe implements Runnable {
        private final Fixture fixture;
        private final AtomicBoolean stop = new AtomicBoolean(false);
        private final AtomicLong reads = new AtomicLong();
        private volatile String failure;

        ReaderProbe(Fixture fixture) {
            this.fixture = fixture;
        }

        @Override
        public void run() {
            for (int i = 0; i < MAX_READER_ITERATIONS && !stop.get(); ++i) {
                final double[] s = sculptState();
                final boolean hasMesh = s[NativeViewport.SCULPT_HAS_MESH] == 1.0;
                final long objectId = (long) s[NativeViewport.SCULPT_OBJECT_ID];
                final int vertices = (int) s[NativeViewport.SCULPT_VERTEX_COUNT];
                final int indices = (int) s[NativeViewport.SCULPT_INDEX_COUNT];
                if (hasMesh) {
                    if (objectId != fixture.a || vertices != fixture.vertexCountA
                            || indices != fixture.indexCountA) {
                        fail("sculptState mixed bodies: hasMesh=1 objectId=" + objectId
                                + " vertices=" + vertices + " indices=" + indices
                                + " (body A is " + fixture.a + " with " + fixture.vertexCountA
                                + "/" + fixture.indexCountA + ")");
                        return;
                    }
                } else if (vertices != 0 || indices != 0) {
                    fail("sculptState reported geometry without a mesh: vertices=" + vertices
                            + " indices=" + indices + " objectId=" + objectId);
                    return;
                }
                if (NativeViewport.productMode() != 0) {
                    fail("productMode left Construction during a read");
                    return;
                }
                if (NativeViewport.sculptTool() != fixture.tool) {
                    fail("sculptTool changed during a read");
                    return;
                }
                final double[] p = primitiveState();
                if (!isBox(p, BASELINE_WIDTH_METERS, BASELINE_HEIGHT_METERS, BASELINE_DEPTH_METERS)
                        && !isBox(p, OTHER_WIDTH_METERS, OTHER_HEIGHT_METERS, OTHER_DEPTH_METERS)) {
                    fail("constructionPrimitive mixed bodies: " + p[1] + "x" + p[2] + "x" + p[3]);
                    return;
                }
                NativeViewport.constructionMeshRevision();
                reads.incrementAndGet();
            }
        }

        private void fail(String why) {
            if (failure == null) {
                failure = why;
            }
        }
    }

    /** Runs the probe around {@code mutate}, then proves it finished and never tore. */
    private void runAgainstReader(Fixture fixture, Runnable mutate) throws InterruptedException {
        final ReaderProbe probe = new ReaderProbe(fixture);
        final Thread reader = new Thread(probe, "sculpt-reader-probe");
        reader.start();
        try {
            mutate.run();
        } finally {
            probe.stop.set(true);
        }
        reader.join(JOIN_TIMEOUT_MS);
        assertFalse("the reader must finish within " + JOIN_TIMEOUT_MS
                + " ms of the mutator: a live thread here is a deadlock", reader.isAlive());
        assertNull(probe.failure, probe.failure);
        assertTrue("the reader must have observed the mutations", probe.reads.get() > 0);
    }

    private void assertBoundToA(Fixture f) {
        final double[] state = sculptState();
        assertEquals("the reader must see body A as the sculpt target", (double) f.a,
                state[NativeViewport.SCULPT_OBJECT_ID], 0.0);
        assertEquals(1.0, state[NativeViewport.SCULPT_HAS_MESH], 0.0);
        assertEquals((double) f.vertexCountA, state[NativeViewport.SCULPT_VERTEX_COUNT], 0.0);
    }

    private static void assertUnbound() {
        final double[] state = sculptState();
        assertEquals("a body that was never frozen reports no mesh", 0.0,
                state[NativeViewport.SCULPT_HAS_MESH], 0.0);
        assertEquals(0.0, state[NativeViewport.SCULPT_VERTEX_COUNT], 0.0);
    }

    // -----------------------------------------------------------------------
    // PAH-R1-03 — body switch vs sculpt-reader query
    // -----------------------------------------------------------------------

    @Test
    public void bodySwitchNeverReturnsCrossBodySculptState() throws InterruptedException {
        final Fixture f = twoBodies();
        runAgainstReader(f, () -> {
            for (int batch = 0; batch < 8; ++batch) {
                doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                    for (int i = 0; i < 50; ++i) {
                        assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(f.b));
                        assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(f.a));
                    }
                    return null;
                });
            }
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(f.a));
            assertBoundToA(f);
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(f.b));
            assertUnbound();
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // PAH-R1-04 — project load vs sculpt-reader query; no stale target after load
    // -----------------------------------------------------------------------

    @Test
    public void projectLoadLeavesNoStaleSculptTarget() throws InterruptedException {
        final Fixture f = twoBodies();
        final byte[] withSculpt = onWorkspace(rule.getScenario(),
                (activity, workspace) -> NativeViewport.encodeProject());
        assertTrue(withSculpt.length > 0);
        runAgainstReader(f, () -> {
            for (int batch = 0; batch < 6; ++batch) {
                doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                    for (int i = 0; i < 5; ++i) {
                        assertEquals(NativeViewport.PROJECT_OK,
                                NativeViewport.loadProject(baselineProject));
                        assertEquals(NativeViewport.PROJECT_OK,
                                NativeViewport.loadProject(withSculpt));
                    }
                    return null;
                });
            }
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            // The last load carried A's frozen mesh and A as the active body.
            assertEquals(f.a, NativeViewport.sceneActiveBodyId());
            assertBoundToA(f);
            // A load of the document WITHOUT the sculpt mesh must not leave the
            // session pointing at the mesh the previous document had.
            assertEquals(NativeViewport.PROJECT_OK, NativeViewport.loadProject(baselineProject));
            assertEquals(f.a, NativeViewport.sceneActiveBodyId());
            assertUnbound();
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // PAH-R1-05 — Delete / Undo vs sculpt-reader query
    // -----------------------------------------------------------------------

    @Test
    public void deleteAndUndoLeaveNoStaleSculptTarget() throws InterruptedException {
        final Fixture f = twoBodies();
        runAgainstReader(f, () -> {
            for (int batch = 0; batch < 6; ++batch) {
                doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                    for (int i = 0; i < 10; ++i) {
                        assertEquals(NativeViewport.DELETE_OK, NativeViewport.sceneDeleteBody(f.a));
                        assertEquals(NativeViewport.HISTORY_OK, NativeViewport.constructionUndo());
                    }
                    return null;
                });
            }
        });
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            // Undo restored the SAME object, sculpt state included; selecting it
            // binds the session to the mesh the history held.
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(f.a));
            assertBoundToA(f);
            assertEquals(NativeViewport.DELETE_OK, NativeViewport.sceneDeleteBody(f.a));
            assertEquals(f.b, NativeViewport.sceneActiveBodyId());
            assertUnbound();
            assertEquals(NativeViewport.HISTORY_OK, NativeViewport.constructionUndo());
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(f.a));
            assertBoundToA(f);
            return null;
        });
    }

    // -----------------------------------------------------------------------
    // PAH-R1-06 — bounded concurrent query / mutation without deadlock
    // -----------------------------------------------------------------------

    @Test
    public void boundedConcurrentReadersAndBrushMutationsComplete() throws InterruptedException {
        final Fixture f = twoBodies();
        final ReaderProbe first = new ReaderProbe(f);
        final ReaderProbe second = new ReaderProbe(f);
        final Thread one = new Thread(first, "sculpt-reader-probe-1");
        final Thread two = new Thread(second, "sculpt-reader-probe-2");
        one.start();
        two.start();
        try {
            for (int batch = 0; batch < 8; ++batch) {
                doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
                    for (int i = 0; i < 50; ++i) {
                        // The brush setters and the tool setter take the lock
                        // and log under it; the tool is set back to what the
                        // probes expect before the batch ends.
                        NativeViewport.setSculptBrush(40.0 + i, 0.5);
                        NativeViewport.setSculptTool(f.tool);
                        assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(f.b));
                        assertEquals(NativeViewport.SCULPT_OK, NativeViewport.sceneSelectBody(f.a));
                    }
                    return null;
                });
            }
        } finally {
            first.stop.set(true);
            second.stop.set(true);
        }
        one.join(JOIN_TIMEOUT_MS);
        two.join(JOIN_TIMEOUT_MS);
        assertFalse("reader 1 must finish: a live thread is a deadlock", one.isAlive());
        assertFalse("reader 2 must finish: a live thread is a deadlock", two.isAlive());
        assertNull(first.failure, first.failure);
        assertNull(second.failure, second.failure);
        assertTrue(first.reads.get() > 0 && second.reads.get() > 0);
    }

    // -----------------------------------------------------------------------
    // PAH-R1-12 / 13 — touchEvent checks each array read before the next JNI call
    // -----------------------------------------------------------------------

    /**
     * A required array shorter than the pointer count makes the region read
     * raise {@code ArrayIndexOutOfBoundsException}. The event must be dropped
     * with the exception cleared and NO further JNI call made while it was
     * pending: under CheckJNI (on by default on the emulator) a second array
     * read with a pending exception aborts the process, so a crash here is the
     * failure and a normal return with the previous event still on record is
     * the pass.
     */
    @Test
    public void touchEventDropsShortRequiredArraysWithoutPendingException() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final float[] out = new float[NativeViewport.POINTER_EVENT_STATE_SIZE];

            // A well-formed one-pointer Down is recorded.
            NativeViewport.touchEvent(MotionEvent.ACTION_DOWN, -1, 1,
                    new int[] {7}, new float[] {5f}, new float[] {6f},
                    null, null, null, null, VIEW_WIDTH, VIEW_HEIGHT);
            assertEquals(1, NativeViewport.debugLastPointerEvent(out));
            assertEquals(7f, out[1 + NativeViewport.POINTER_SAMPLE_ID], 0f);
            assertEquals(5f, out[1 + NativeViewport.POINTER_SAMPLE_X], 0f);
            assertEquals(6f, out[1 + NativeViewport.POINTER_SAMPLE_Y], 0f);

            // ids too short for two pointers: the first read raises.
            NativeViewport.touchEvent(MotionEvent.ACTION_MOVE, -1, 2,
                    new int[] {7}, new float[] {50f, 60f}, new float[] {70f, 80f},
                    null, null, null, null, VIEW_WIDTH, VIEW_HEIGHT);
            assertEquals("a dropped event leaves the last recorded one in place",
                    1, NativeViewport.debugLastPointerEvent(out));
            assertEquals(5f, out[1 + NativeViewport.POINTER_SAMPLE_X], 0f);

            // xs too short: the SECOND read raises, after a successful first.
            NativeViewport.touchEvent(MotionEvent.ACTION_MOVE, -1, 2,
                    new int[] {7, 8}, new float[] {50f}, new float[] {70f, 80f},
                    null, null, null, null, VIEW_WIDTH, VIEW_HEIGHT);
            assertEquals(1, NativeViewport.debugLastPointerEvent(out));
            assertEquals(5f, out[1 + NativeViewport.POINTER_SAMPLE_X], 0f);

            // ys too short: the THIRD read raises, after two successful ones.
            NativeViewport.touchEvent(MotionEvent.ACTION_MOVE, -1, 2,
                    new int[] {7, 8}, new float[] {50f, 60f}, new float[] {70f},
                    null, null, null, null, VIEW_WIDTH, VIEW_HEIGHT);
            assertEquals(1, NativeViewport.debugLastPointerEvent(out));
            assertEquals(5f, out[1 + NativeViewport.POINTER_SAMPLE_X], 0f);

            // The boundary still works afterwards, and the gesture is closed.
            NativeViewport.touchEvent(MotionEvent.ACTION_MOVE, -1, 1,
                    new int[] {7}, new float[] {9f}, new float[] {10f},
                    null, null, null, null, VIEW_WIDTH, VIEW_HEIGHT);
            assertEquals(1, NativeViewport.debugLastPointerEvent(out));
            assertEquals(9f, out[1 + NativeViewport.POINTER_SAMPLE_X], 0f);
            NativeViewport.touchEvent(MotionEvent.ACTION_CANCEL, -1, 1,
                    new int[] {7}, new float[] {9f}, new float[] {10f},
                    null, null, null, null, VIEW_WIDTH, VIEW_HEIGHT);
            return null;
        });
    }
}
