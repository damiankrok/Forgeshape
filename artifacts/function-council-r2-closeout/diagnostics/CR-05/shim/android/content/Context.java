package android.content;

import java.io.File;

/** CR-05 sandbox shim. NOT android.content.Context: only the two calls the
 *  product classes under test make (getFilesDir, getApplicationContext),
 *  pointed at a throw-away temp directory so no real user file is touched. */
public class Context {
    private final File filesDir;
    public Context(File filesDir) { this.filesDir = filesDir; }
    public File getFilesDir() { return filesDir; }
    public Context getApplicationContext() { return this; }
}
