package com.forgeshape.app;

import static com.forgeshape.app.WorkspaceTestSupport.doOnWorkspace;
import static com.forgeshape.app.WorkspaceTestSupport.resetToBaselineConstruction;
import static com.forgeshape.app.WorkspaceTestSupport.settleLayout;
import static org.junit.Assert.assertTrue;

import android.os.SystemClock;

import androidx.test.core.app.ActivityScenario;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * The renderer's diagnostic counters are PROCESS-lifetime counts, and a render
 * thread restart must never make one run backwards.
 *
 * <p><b>Why this exists.</b> Every Activity that finishes for real stops the
 * render thread, and the next one starts a new thread with a new
 * {@code Renderer} whose own counters begin at zero. The UI thread reads them
 * through process-scoped mirrors ({@link NativeViewport#selectionOutlineStats},
 * whose allocation slot is documented "for the life of the process"). If a
 * mirror simply copies the new renderer's count, it holds the DEAD renderer's
 * total until the new renderer's first frame and then drops to 1 — so a test
 * that reads its baseline in that window waits for a target the live renderer
 * cannot reach, and fails for a reason that has nothing to do with the outline.
 * That is exactly how `SelectionOutlineTest`'s "the renderer records the
 * composite draw" failed on the CI emulator, where SwiftShader presents about
 * three frames a second.
 *
 * <p>This case drives that window on purpose: it lets one renderer draw
 * outlined frames, finishes the Activity so the thread stops, starts a new one,
 * and reads the counters from the first instant until the live renderer has
 * drawn past the dead one's total. Every read must be at least the one before
 * it — for the three outline counters, and for the presented-frame and
 * device-rebuild counts that are mirrored on the same terms. The Activities are launched by hand rather than through an
 * {@code ActivityScenarioRule}, because the restart between them IS the
 * subject.
 */
@RunWith(AndroidJUnit4.class)
public final class RendererCounterContinuityTest {

    /** A hard safety bound around each event wait below; never a pacing delay. */
    private static final long EVENT_BOUND_MS = 20_000L;
    /** How often the mirrors are read while waiting for the live renderer. */
    private static final long POLL_MS = 25L;

    private static final int[] COUNTERS = {
            NativeViewport.OUTLINE_STAT_MASK_ALLOCATIONS,
            NativeViewport.OUTLINE_STAT_MASK_PASS_FRAMES,
            NativeViewport.OUTLINE_STAT_COMPOSITE_DRAWS,
    };
    /** Two more renderer counters mirrored on the same terms, read beside them. */
    private static final int FRAMES_PRESENTED = 0;
    private static final int DEVICE_REBUILDS = 1;
    private static final String[] RENDERER_COUNTER_NAMES = {"framesPresented", "deviceRebuilds"};

    @Test
    public void outlineCountersNeverRunBackwardsAcrossARenderThreadRestart() {
        // --- A renderer that has drawn outlined frames -----------------------
        try (ActivityScenario<ForgeShapeActivity> first =
                     ActivityScenario.launch(ForgeShapeActivity.class)) {
            openAnOutlinedProject(first);
            // Four STRICT increases of the composite counter, each one a frame
            // this renderer recorded. Counting increases rather than comparing
            // with a first read keeps the wait honest even when the first read
            // is a mirror left over from an earlier Activity.
            final int increases = awaitCompositeIncreases(4);
            assertTrue("the first renderer recorded outlined frames: " + increases
                    + " composite increase(s) within " + EVENT_BOUND_MS + " ms", increases >= 4);
        }
        // The Activity is DESTROYED and, because it was not a configuration
        // change, NativeViewport.stop() has joined the render thread. Nothing
        // writes the mirrors now, so this is the dead renderer's last word.
        final double[] stopped = outlineStats();
        final long[] rendererStopped = rendererCounters();

        // --- A new render thread ---------------------------------------------
        try (ActivityScenario<ForgeShapeActivity> second =
                     ActivityScenario.launch(ForgeShapeActivity.class)) {
            double[] previous = stopped;
            long[] rendererPrevious = rendererStopped;
            final double target = stopped[NativeViewport.OUTLINE_STAT_COMPOSITE_DRAWS] + 3.0;
            boolean projectReady = false;
            final long began = SystemClock.uptimeMillis();
            while (SystemClock.uptimeMillis() - began < EVENT_BOUND_MS) {
                final double[] now = outlineStats();
                for (int slot : COUNTERS) {
                    assertTrue("renderer counter slot " + slot + " ran backwards across a render"
                                    + " thread restart: " + previous[slot] + " -> " + now[slot]
                                    + " (the stopped renderer's last value was " + stopped[slot]
                                    + ", read " + (SystemClock.uptimeMillis() - began)
                                    + " ms after the relaunch)",
                            now[slot] >= previous[slot]);
                }
                final long[] rendererNow = rendererCounters();
                for (int i = 0; i < rendererNow.length; i++) {
                    assertTrue("renderer counter " + RENDERER_COUNTER_NAMES[i] + " ran backwards"
                                    + " across a render thread restart: " + rendererPrevious[i]
                                    + " -> " + rendererNow[i] + " (the stopped renderer's last"
                                    + " value was " + rendererStopped[i] + ")",
                            rendererNow[i] >= rendererPrevious[i]);
                }
                previous = now;
                rendererPrevious = rendererNow;
                if (now[NativeViewport.OUTLINE_STAT_COMPOSITE_DRAWS] >= target) {
                    break;
                }
                // The project is re-established once, AFTER the first reads, so
                // the window between the relaunch and the new renderer's first
                // frame is observed rather than skipped.
                if (!projectReady) {
                    openAnOutlinedProject(second);
                    projectReady = true;
                    continue;
                }
                SystemClock.sleep(POLL_MS);
            }
            android.util.Log.i("ForgeShape", "FORGESHAPE_RENDER_COUNTER_CONTINUITY"
                    + " stoppedComposite=" + (long) stopped[NativeViewport.OUTLINE_STAT_COMPOSITE_DRAWS]
                    + " liveComposite=" + (long) previous[NativeViewport.OUTLINE_STAT_COMPOSITE_DRAWS]
                    + " stoppedMaskPass=" + (long) stopped[NativeViewport.OUTLINE_STAT_MASK_PASS_FRAMES]
                    + " liveMaskPass=" + (long) previous[NativeViewport.OUTLINE_STAT_MASK_PASS_FRAMES]
                    + " stoppedAllocations=" + (long) stopped[NativeViewport.OUTLINE_STAT_MASK_ALLOCATIONS]
                    + " liveAllocations=" + (long) previous[NativeViewport.OUTLINE_STAT_MASK_ALLOCATIONS]
                    + " stoppedFramesPresented=" + rendererStopped[FRAMES_PRESENTED]
                    + " liveFramesPresented=" + rendererPrevious[FRAMES_PRESENTED]);
            assertTrue("the live renderer drew past the stopped renderer's total within "
                            + EVENT_BOUND_MS + " ms: stopped="
                            + stopped[NativeViewport.OUTLINE_STAT_COMPOSITE_DRAWS] + " live="
                            + previous[NativeViewport.OUTLINE_STAT_COMPOSITE_DRAWS],
                    previous[NativeViewport.OUTLINE_STAT_COMPOSITE_DRAWS] >= target);
        }
    }

    /** A Construction project with its body selected and the outline on. */
    private static void openAnOutlinedProject(ActivityScenario<ForgeShapeActivity> scenario) {
        resetToBaselineConstruction(scenario);
        doOnWorkspace(scenario, (activity, workspace) -> {
            NativeViewport.setSelectionOutlineVisible(true);
            return null;
        });
        settleLayout();
    }

    /** Counts strict increases of the composite counter, up to {@code wanted}. */
    private static int awaitCompositeIncreases(int wanted) {
        int increases = 0;
        double last = outlineStats()[NativeViewport.OUTLINE_STAT_COMPOSITE_DRAWS];
        final long began = SystemClock.uptimeMillis();
        while (increases < wanted && SystemClock.uptimeMillis() - began < EVENT_BOUND_MS) {
            SystemClock.sleep(POLL_MS);
            final double now = outlineStats()[NativeViewport.OUTLINE_STAT_COMPOSITE_DRAWS];
            if (now > last) {
                increases++;
            }
            last = now;
        }
        return increases;
    }

    private static long[] rendererCounters() {
        final long[] counters = new long[RENDERER_COUNTER_NAMES.length];
        counters[FRAMES_PRESENTED] = NativeViewport.debugRendererFramesPresented();
        counters[DEVICE_REBUILDS] = NativeViewport.debugRendererDeviceRebuilds();
        return counters;
    }

    private static double[] outlineStats() {
        final double[] stats = new double[NativeViewport.OUTLINE_STATS_SIZE];
        NativeViewport.selectionOutlineStats(stats);
        return stats;
    }
}
