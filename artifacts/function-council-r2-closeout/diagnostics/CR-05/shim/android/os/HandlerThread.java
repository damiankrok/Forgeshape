package android.os;
/** CR-05 sandbox shim. */
public class HandlerThread extends Thread {
    private final Looper looper = new Looper();
    public HandlerThread(String name) { super(name); setDaemon(true); }
    @Override public void run() { looper.loop(); }
    public Looper getLooper() { return looper; }
    public boolean quitSafely() { looper.quitSafely(); return true; }
}
