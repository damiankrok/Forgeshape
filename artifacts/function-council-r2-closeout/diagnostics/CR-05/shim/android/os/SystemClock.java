package android.os;
/** CR-05 sandbox shim: monotonic millis. */
public final class SystemClock {
    private SystemClock() {}
    public static long uptimeMillis() { return System.nanoTime() / 1_000_000L; }
}
