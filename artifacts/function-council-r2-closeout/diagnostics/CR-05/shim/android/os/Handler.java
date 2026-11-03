package android.os;
/** CR-05 sandbox shim: post / postDelayed / removeCallbacks onto a Looper. */
public class Handler {
    private final Looper looper;
    public Handler(Looper looper) { this.looper = looper; }
    public final boolean post(Runnable r) { return postDelayed(r, 0L); }
    public final boolean postDelayed(Runnable r, long delayMillis) {
        return looper.enqueue(r, SystemClock.uptimeMillis() + Math.max(0L, delayMillis));
    }
    public final void removeCallbacks(Runnable r) { looper.remove(r); }
}
