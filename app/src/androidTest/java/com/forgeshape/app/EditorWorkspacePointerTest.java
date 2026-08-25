package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.describeSnapshotDifference;
import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.nativeSnapshot;
import static com.forgeshape.app.WorkspaceTestSupport.onWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settle;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * The pointer boundary, end to end: MotionEvent to native.
 *
 * <p>Two things are proved here and nothing else. First, that a tool type, a
 * pressure and a tilt survive the whole crossing — MotionEvent, SurfaceView,
 * JNI, platform-neutral C++ — and stay attached to the pointer they belong to.
 * Second, that carrying them changes <b>nothing</b>: tap selection, camera
 * navigation and the pending-then-promote sculpt arbitration behave exactly as
 * they did before the stylus fields existed, and two strokes over the same
 * pixels at opposite pressures leave native state identical.
 *
 * <p>No physical stylus is required. Every stylus event here is synthesised with
 * {@link MotionEvent#obtain(long, long, int, int,
 * MotionEvent.PointerProperties[], MotionEvent.PointerCoords[], int, int, float,
 * float, int, int, int, int)}, which is the only way to set a tool type and an
 * axis value from a test; a real S Pen would exercise the same path.
 *
 * <p>Native data is read back through {@code NativeViewport.debugLastPointerEvent},
 * a debug-only diagnostic hook. It has no product surface, holds no truth and
 * does not exist in a release build — see its declaration.
 */
@RunWith(AndroidJUnit4.class)
public final class EditorWorkspacePointerTest {

    @Rule
    public ActivityScenarioRule<ForgeShapeActivity> rule =
            new ActivityScenarioRule<>(ForgeShapeActivity.class);

    /** Angles are floats through JNI, so an exact compare would be a coin toss. */
    private static final float ANGLE_TOLERANCE = 1.0e-4f;

    /** Pressure survives as a float too, but nothing rescales it. */
    private static final float PRESSURE_TOLERANCE = 1.0e-6f;

    @Before
    public void startFromABaselineBox() {
        resetToBaselineConstruction(rule.getScenario());
    }

    // -----------------------------------------------------------------------
    // INR1-11..15 -- tool type reaches native code
    // -----------------------------------------------------------------------

    @Test
    public void inr111_aFingerArrivesAsTheNeutralFinger() {
        final float[] sample = dispatchSinglePointer(MotionEvent.TOOL_TYPE_FINGER,
                0.5f, 0.0f, 0.0f);
        assertEquals("a finger must still be a finger on the native side",
                PointerSemantics.TOOL_FINGER, toolTypeOf(sample, 0));
    }

    @Test
    public void inr112_aStylusArrivesAsTheNeutralStylus() {
        final float[] sample = dispatchSinglePointer(MotionEvent.TOOL_TYPE_STYLUS,
                0.5f, 0.3f, 0.2f);
        assertEquals(PointerSemantics.TOOL_STYLUS, toolTypeOf(sample, 0));
    }

    @Test
    public void inr113_stylusPressureReachesNativeUnchanged() {
        final float[] light = dispatchSinglePointer(MotionEvent.TOOL_TYPE_STYLUS,
                0.125f, 0.0f, 0.0f);
        assertEquals("pressure crosses the boundary unrescaled",
                0.125f, pressureOf(light, 0), PRESSURE_TOLERANCE);

        final float[] heavy = dispatchSinglePointer(MotionEvent.TOOL_TYPE_STYLUS,
                0.875f, 0.0f, 0.0f);
        assertEquals(0.875f, pressureOf(heavy, 0), PRESSURE_TOLERANCE);
        assertNotEquals("the two profiles must actually differ",
                pressureOf(light, 0), pressureOf(heavy, 0), PRESSURE_TOLERANCE);
    }

    /**
     * Out-of-contract pressure is repaired, not rejected. Some hardware reports
     * more than 1.0, and native code owns the range, so the event still lands
     * and the value arrives clamped.
     */
    @Test
    public void inr113_outOfRangePressureIsClampedRatherThanDropped() {
        final float[] sample = dispatchSinglePointer(MotionEvent.TOOL_TYPE_STYLUS,
                4.0f, 0.0f, 0.0f);
        assertEquals(1, countOf(sample));
        assertEquals("native owns the [0, 1] range", 1.0f, pressureOf(sample, 0),
                PRESSURE_TOLERANCE);
    }

    @Test
    public void inr114_stylusTiltAndOrientationReachNativeInRadians() {
        final float tilt = 0.7f;
        final float orientation = -1.1f;
        final float[] sample = dispatchSinglePointer(MotionEvent.TOOL_TYPE_STYLUS,
                0.6f, tilt, orientation);
        assertEquals("tilt is radians from perpendicular, carried as-is",
                tilt, tiltOf(sample, 0), ANGLE_TOLERANCE);
        assertEquals("orientation is radians in the screen plane, carried as-is",
                orientation, tiltOrientationOf(sample, 0), ANGLE_TOLERANCE);
    }

    /**
     * The pairing rule, observed end to end: a pointer with no lean reports no
     * lean direction, which is what stops a finger's touch-ellipse orientation
     * from arriving as a stylus azimuth that means nothing.
     */
    @Test
    public void inr114_aPointerWithoutTiltCarriesNoTiltDirection() {
        final float[] sample = dispatchSinglePointer(MotionEvent.TOOL_TYPE_FINGER,
                0.5f, 0.0f, 1.2f);
        assertEquals(0.0f, tiltOf(sample, 0), ANGLE_TOLERANCE);
        assertEquals("no lean means no lean direction",
                0.0f, tiltOrientationOf(sample, 0), ANGLE_TOLERANCE);
    }

    @Test
    public void inr115_anUnknownToolTypeArrivesSafelyAsUnknown() {
        final float[] sample = dispatchSinglePointer(MotionEvent.TOOL_TYPE_UNKNOWN,
                0.4f, 0.0f, 0.0f);
        assertEquals(PointerSemantics.TOOL_UNKNOWN, toolTypeOf(sample, 0));
        assertEquals("an unknown tool is still a pointer, not a dropped event",
                1, countOf(sample));
        assertEquals("and it keeps the default pressure fallback rules", 0.4f,
                pressureOf(sample, 0), PRESSURE_TOLERANCE);
    }

    // -----------------------------------------------------------------------
    // INR1-16 -- per-pointer association
    // -----------------------------------------------------------------------

    /**
     * A stylus and a finger down at once, with deliberately different pressures,
     * tilts and positions. Every field has to stay with its own pointer: a
     * packing bug that shifted one array by a slot would show up here and
     * nowhere else.
     */
    @Test
    public void inr116_multiPointerEventKeepsEachPointersDataWithThatPointer() {
        final float[] sample = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = viewportOf(workspace);
            final long when = SystemClock.uptimeMillis();

            final MotionEvent.PointerProperties[] props = {
                    pointerProperties(11, MotionEvent.TOOL_TYPE_STYLUS),
                    pointerProperties(22, MotionEvent.TOOL_TYPE_FINGER)};
            final MotionEvent.PointerCoords[] coords = {
                    pointerCoords(120.0f, 240.0f, 0.2f, 0.9f, 1.4f),
                    pointerCoords(300.0f, 480.0f, 0.8f, 0.0f, 0.0f)};

            dispatch(viewport, when, when, MotionEvent.ACTION_DOWN, props, coords, 1);
            dispatch(viewport, when, when + 16L, MotionEvent.ACTION_MOVE, props, coords, 2);
            final float[] read = readLastPointerEvent();
            dispatch(viewport, when, when + 32L, MotionEvent.ACTION_UP, props, coords, 2);
            return read;
        });

        assertEquals("both pointers must have crossed", 2, countOf(sample));

        assertEquals(11, idOf(sample, 0));
        assertEquals(PointerSemantics.TOOL_STYLUS, toolTypeOf(sample, 0));
        assertEquals(120.0f, xOf(sample, 0), 0.5f);
        assertEquals(240.0f, yOf(sample, 0), 0.5f);
        assertEquals(0.2f, pressureOf(sample, 0), PRESSURE_TOLERANCE);
        assertEquals(0.9f, tiltOf(sample, 0), ANGLE_TOLERANCE);
        assertEquals(1.4f, tiltOrientationOf(sample, 0), ANGLE_TOLERANCE);

        assertEquals(22, idOf(sample, 1));
        assertEquals(PointerSemantics.TOOL_FINGER, toolTypeOf(sample, 1));
        assertEquals(300.0f, xOf(sample, 1), 0.5f);
        assertEquals(480.0f, yOf(sample, 1), 0.5f);
        assertEquals(0.8f, pressureOf(sample, 1), PRESSURE_TOLERANCE);
        assertEquals("the finger has no lean", 0.0f, tiltOf(sample, 1), ANGLE_TOLERANCE);
    }

    // -----------------------------------------------------------------------
    // INR1-17..20 -- nothing about the existing gestures moved
    // -----------------------------------------------------------------------

    /**
     * INR1-17. A tap still resolves a tap. Selection has no direct read-back, so
     * this asserts what the touch path is FOR: the gesture is consumed by the
     * viewport, and it leaves Construction and sculpt truth untouched.
     */
    @Test
    public void inr117_tapSelectionIsUnchanged() {
        final double[] before = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());

        final Boolean consumed = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = viewportOf(workspace);
            final long when = SystemClock.uptimeMillis();
            final float cx = viewport.getWidth() * 0.5f;
            final float cy = viewport.getHeight() * 0.5f;
            boolean took = sendFinger(viewport, when, when, MotionEvent.ACTION_DOWN, cx, cy);
            took &= sendFinger(viewport, when, when + 16L, MotionEvent.ACTION_UP, cx, cy);
            return took;
        });
        assertTrue("the viewport still owns its own tap", Boolean.TRUE.equals(consumed));

        final double[] after = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());
        assertArrayEquals("a tap changes no geometry:"
                + describeSnapshotDifference(before, after), before, after, 0.0);
    }

    /**
     * INR1-18. One-finger navigation still orbits, and orbiting is still not an
     * edit: no Construction parameter, no transform and no sculpt revision moves.
     */
    @Test
    public void inr118_cameraNavigationIsUnchanged() {
        final double[] before = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            dragViewportWithAFinger(viewportOf(workspace));
            return null;
        });
        settle();

        final double[] after = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());
        assertArrayEquals("navigation is not an edit:"
                + describeSnapshotDifference(before, after), before, after, 0.0);
    }

    /**
     * INR1-19. The pending-then-promote rule still holds. A finger that lands on
     * the Frozen Sculpt Mesh and travels far enough deforms it; a finger that
     * lands and lifts without travelling leaves it bit-identical. Both are read
     * from the native has-edits flag, which is the authority.
     */
    @Test
    public void inr119_sculptPendingThenPromoteIsUnchanged() {
        freezeASculptableMesh();

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = viewportOf(workspace);
            final long when = SystemClock.uptimeMillis();
            final float cx = viewport.getWidth() * 0.5f;
            final float cy = viewport.getHeight() * 0.5f;
            // Down and straight back up: pending, never promoted.
            sendFinger(viewport, when, when, MotionEvent.ACTION_DOWN, cx, cy);
            sendFinger(viewport, when, when + 16L, MotionEvent.ACTION_UP, cx, cy);
            return null;
        });
        assertTrue("a gesture that never travelled must not have sculpted",
                !currentMeshHasEdits());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            strokeViewport(viewportOf(workspace), MotionEvent.TOOL_TYPE_FINGER, 0.5f);
            return null;
        });
        assertTrue("a gesture that travelled past the promotion threshold sculpts",
                currentMeshHasEdits());
    }

    /**
     * INR1-20. Two-finger navigation still cannot mutate the sculpt mesh, even
     * when the fingers land right on it and even when the first one is a stylus
     * pressing as hard as the contract allows.
     */
    @Test
    public void inr120_twoPointerNavigationStillNeverSculpts() {
        freezeASculptableMesh();
        final double[] before = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());

        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = viewportOf(workspace);
            final long when = SystemClock.uptimeMillis();
            final float cx = viewport.getWidth() * 0.5f;
            final float cy = viewport.getHeight() * 0.5f;

            final MotionEvent.PointerProperties[] props = {
                    pointerProperties(0, MotionEvent.TOOL_TYPE_STYLUS),
                    pointerProperties(1, MotionEvent.TOOL_TYPE_STYLUS)};
            final MotionEvent.PointerCoords[] first = {
                    pointerCoords(cx, cy, 1.0f, 1.2f, 0.4f),
                    pointerCoords(cx + 60.0f, cy, 1.0f, 1.2f, 0.4f)};

            // First finger down alone, then the second: exactly the sequence
            // that makes the first one PENDING before the gesture turns out to
            // be navigation.
            dispatch(viewport, when, when, MotionEvent.ACTION_DOWN, props, first, 1);
            dispatch(viewport, when, when + 16L,
                    MotionEvent.ACTION_POINTER_DOWN
                            | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT),
                    props, first, 2);
            for (int step = 1; step <= 6; step++) {
                final MotionEvent.PointerCoords[] moved = {
                        pointerCoords(cx - step * 14.0f, cy, 1.0f, 1.2f, 0.4f),
                        pointerCoords(cx + 60.0f + step * 14.0f, cy, 1.0f, 1.2f, 0.4f)};
                dispatch(viewport, when, when + 16L * (step + 1), MotionEvent.ACTION_MOVE,
                        props, moved, 2);
            }
            dispatch(viewport, when, when + 200L,
                    MotionEvent.ACTION_POINTER_UP
                            | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT),
                    props, first, 2);
            dispatch(viewport, when, when + 216L, MotionEvent.ACTION_UP, props, first, 1);
            return null;
        });
        settle();

        assertTrue("multi-touch navigation must never sculpt", !currentMeshHasEdits());
        final double[] after = onWorkspace(rule.getScenario(),
                (activity, workspace) -> nativeSnapshot());
        assertArrayEquals("multi-touch navigation minted no revision:"
                + describeSnapshotDifference(before, after), before, after, 0.0);
    }

    // -----------------------------------------------------------------------
    // INR1-21 -- the brush is pressure-independent, on a real device
    // -----------------------------------------------------------------------

    /**
     * The same stroke geometry, twice, at opposite ends of the pressure range and
     * with opposite tilts, each on a freshly frozen mesh. Native sculpt state —
     * revision, vertex and index counts, the has-edits flag, radius, strength,
     * tool — must come out identical.
     *
     * <p>Vertex positions have no JNI read-back, so bit-exact vertex equality is
     * asserted by the native self-test that runs on this same build; what this
     * case adds is that the whole device path, MotionEvent included, produces the
     * same outcome.
     */
    @Test
    public void inr121_pressureDoesNotChangeWhatAStrokeProduces() {
        freezeASculptableMesh();
        final double[] light = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            strokeViewport(viewportOf(workspace), MotionEvent.TOOL_TYPE_STYLUS, 0.02f);
            return nativeSnapshot();
        });

        freezeASculptableMesh();
        final double[] heavy = onWorkspace(rule.getScenario(), (activity, workspace) -> {
            strokeViewport(viewportOf(workspace), MotionEvent.TOOL_TYPE_STYLUS, 1.0f);
            return nativeSnapshot();
        });

        assertTrue("precondition: the stroke must actually have edited something",
                heavy[NativeViewport.PRIMITIVE_STATE_SIZE + 6 + NativeViewport.SCULPT_HAS_EDITS]
                        != 0.0);
        assertArrayEquals("pressure must not change the result of a stroke:"
                + describeSnapshotDifference(light, heavy), light, heavy, 0.0);
    }

    // -----------------------------------------------------------------------
    // Support
    // -----------------------------------------------------------------------

    private static View viewportOf(EditorWorkspaceView workspace) {
        final View viewport = workspace.findViewById(R.id.viewport_surface);
        assertTrue("precondition: the viewport must be laid out",
                viewport != null && viewport.getWidth() > 0 && viewport.getHeight() > 0);
        return viewport;
    }

    /** Sends one synthetic pointer through the real view and reads native back. */
    private float[] dispatchSinglePointer(final int androidToolType, final float pressure,
                                          final float tilt, final float orientation) {
        return onWorkspace(rule.getScenario(), (activity, workspace) -> {
            final View viewport = viewportOf(workspace);
            final long when = SystemClock.uptimeMillis();
            final float cx = viewport.getWidth() * 0.5f;
            final float cy = viewport.getHeight() * 0.5f;
            final MotionEvent.PointerProperties[] props = {pointerProperties(0, androidToolType)};
            final MotionEvent.PointerCoords[] coords = {
                    pointerCoords(cx, cy, pressure, tilt, orientation)};

            dispatch(viewport, when, when, MotionEvent.ACTION_DOWN, props, coords, 1);
            final float[] read = readLastPointerEvent();
            dispatch(viewport, when, when + 16L, MotionEvent.ACTION_UP, props, coords, 1);
            return read;
        });
    }

    private static float[] readLastPointerEvent() {
        final float[] out = new float[NativeViewport.POINTER_EVENT_STATE_SIZE];
        final int count = NativeViewport.debugLastPointerEvent(out);
        assertTrue("the debug pointer hook must be available in a debug build", count >= 0);
        return out;
    }

    private static int countOf(float[] sample) {
        return (int) sample[NativeViewport.POINTER_EVENT_COUNT];
    }

    private static int idOf(float[] sample, int index) {
        return (int) sample[NativeViewport.pointerSampleBase(index)
                + NativeViewport.POINTER_SAMPLE_ID];
    }

    private static float xOf(float[] sample, int index) {
        return sample[NativeViewport.pointerSampleBase(index) + NativeViewport.POINTER_SAMPLE_X];
    }

    private static float yOf(float[] sample, int index) {
        return sample[NativeViewport.pointerSampleBase(index) + NativeViewport.POINTER_SAMPLE_Y];
    }

    private static int toolTypeOf(float[] sample, int index) {
        return (int) sample[NativeViewport.pointerSampleBase(index)
                + NativeViewport.POINTER_SAMPLE_TOOL_TYPE];
    }

    private static float pressureOf(float[] sample, int index) {
        return sample[NativeViewport.pointerSampleBase(index)
                + NativeViewport.POINTER_SAMPLE_PRESSURE];
    }

    private static float tiltOf(float[] sample, int index) {
        return sample[NativeViewport.pointerSampleBase(index) + NativeViewport.POINTER_SAMPLE_TILT];
    }

    private static float tiltOrientationOf(float[] sample, int index) {
        return sample[NativeViewport.pointerSampleBase(index)
                + NativeViewport.POINTER_SAMPLE_TILT_ORIENTATION];
    }

    private static MotionEvent.PointerProperties pointerProperties(int id, int toolType) {
        final MotionEvent.PointerProperties props = new MotionEvent.PointerProperties();
        props.id = id;
        props.toolType = toolType;
        return props;
    }

    private static MotionEvent.PointerCoords pointerCoords(float x, float y, float pressure,
                                                           float tilt, float orientation) {
        final MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
        coords.x = x;
        coords.y = y;
        coords.pressure = pressure;
        coords.size = 1.0f;
        coords.setAxisValue(MotionEvent.AXIS_TILT, tilt);
        coords.orientation = orientation;
        return coords;
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

    /** The plain finger path every pre-existing gesture used. */
    private static boolean sendFinger(View viewport, long downTime, long eventTime, int action,
                                      float x, float y) {
        final MotionEvent event = MotionEvent.obtain(downTime, eventTime, action, x, y, 0);
        try {
            return viewport.dispatchTouchEvent(event);
        } finally {
            event.recycle();
        }
    }

    private static void dragViewportWithAFinger(View viewport) {
        final long when = SystemClock.uptimeMillis();
        final float cx = viewport.getWidth() * 0.25f;
        final float cy = viewport.getHeight() * 0.25f;
        sendFinger(viewport, when, when, MotionEvent.ACTION_DOWN, cx, cy);
        for (int step = 1; step <= 6; step++) {
            sendFinger(viewport, when, when + 16L * step, MotionEvent.ACTION_MOVE,
                    cx + step * 10.0f, cy + step * 6.0f);
        }
        sendFinger(viewport, when, when + 128L, MotionEvent.ACTION_UP, cx + 60.0f, cy + 36.0f);
    }

    /**
     * A stroke down the middle of the viewport with one synthetic pointer, at a
     * fixed pressure. The geometry is identical whatever the tool type or the
     * pressure, which is what makes two runs comparable.
     */
    private static void strokeViewport(View viewport, int androidToolType, float pressure) {
        final long when = SystemClock.uptimeMillis();
        final float cx = viewport.getWidth() * 0.5f;
        final float cy = viewport.getHeight() * 0.5f;
        // A tilt that differs with the pressure, so this varies both halves of
        // the stylus contract at once rather than only one.
        final float tilt = pressure > 0.5f ? 1.2f : 0.0f;
        final MotionEvent.PointerProperties[] props = {pointerProperties(0, androidToolType)};

        dispatch(viewport, when, when, MotionEvent.ACTION_DOWN, props,
                new MotionEvent.PointerCoords[]{pointerCoords(cx, cy, pressure, tilt, 0.5f)}, 1);
        for (int step = 1; step <= 8; step++) {
            dispatch(viewport, when, when + 16L * step, MotionEvent.ACTION_MOVE, props,
                    new MotionEvent.PointerCoords[]{
                            pointerCoords(cx + step * 6.0f, cy + step * 4.0f, pressure, tilt,
                                    0.5f)}, 1);
        }
        dispatch(viewport, when, when + 160L, MotionEvent.ACTION_UP, props,
                new MotionEvent.PointerCoords[]{
                        pointerCoords(cx + 48.0f, cy + 32.0f, pressure, tilt, 0.5f)}, 1);
    }

    /** Puts the product in Sculpt mode on a mesh a centre stroke can reach. */
    private void freezeASculptableMesh() {
        doOnWorkspace(rule.getScenario(), (activity, workspace) -> {
            NativeViewport.enterConstructionMode();
            NativeViewport.applyConstructionSphere(2.0);
            NativeViewport.applyBoxTransform(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
            NativeViewport.setSculptTool(NativeViewport.TOOL_GRAB);
            NativeViewport.setSculptBrush(300.0, 1.0);
            assertEquals(NativeViewport.SCULPT_OK, NativeViewport.freezeToSculpt());
            workspace.syncFromNative();
            return null;
        });
        settle();
    }

    private static boolean currentMeshHasEdits() {
        final double[] sculpt = new double[NativeViewport.SCULPT_STATE_SIZE];
        NativeViewport.sculptState(sculpt);
        return sculpt[NativeViewport.SCULPT_HAS_EDITS] != 0.0;
    }
}
