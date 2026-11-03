package android.os;

import java.util.ArrayList;
import java.util.List;

/** CR-05 sandbox shim: a single-thread message queue with Android's ordering
 *  (by due time, then FIFO), removeCallbacks, and quitSafely semantics (run
 *  every message already due, then exit). */
public final class Looper {
    private static final class Msg {
        final Runnable r; final long when; final long seq;
        Msg(Runnable r, long when, long seq) { this.r = r; this.when = when; this.seq = seq; }
    }
    private final List<Msg> queue = new ArrayList<>();
    private long seq;
    private boolean quitting;

    synchronized boolean enqueue(Runnable r, long when) {
        if (quitting) return false;
        queue.add(new Msg(r, when, seq++));
        notifyAll();
        return true;
    }
    synchronized void remove(Runnable r) {
        queue.removeIf(m -> m.r == r);
    }
    synchronized void quitSafely() { quitting = true; notifyAll(); }

    private synchronized Msg next() throws InterruptedException {
        while (true) {
            Msg best = null;
            for (Msg m : queue) {
                if (best == null || m.when < best.when || (m.when == best.when && m.seq < best.seq)) best = m;
            }
            final long now = SystemClock.uptimeMillis();
            if (best != null && best.when <= now) { queue.remove(best); return best; }
            if (quitting) return null;   // quitSafely: nothing due now -> exit
            if (best == null) wait(); else wait(Math.max(1L, best.when - now));
        }
    }
    void loop() {
        try {
            Msg m;
            while ((m = next()) != null) m.r.run();
        } catch (InterruptedException e) { Thread.currentThread().interrupt(); }
    }
}
