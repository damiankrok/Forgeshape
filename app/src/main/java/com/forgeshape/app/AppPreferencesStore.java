package com.forgeshape.app;

import android.content.Context;
import android.content.SharedPreferences;

/**
 * Where {@link AppPreferences} live between runs (`UI-PREF-R1` B2), and the
 * one process-scoped copy every layer reads.
 *
 * <p><b>The smallest Android-native mechanism that survives everything the
 * stage asks for.</b> {@code SharedPreferences} is platform code the app
 * already ships with — no new dependency — and it writes its file atomically
 * (a backup, then a rename), which is enough for a record of five fields. Writes
 * are {@code commit()}ed synchronously: a preference changes a handful of times
 * in a session, the file is a few hundred bytes, and a synchronous write is
 * what lets a case assert "on disk" the moment the call returns rather than
 * after a debounce it would have to sleep for.
 *
 * <p><b>Loaded before anything is drawn.</b> The Activity reads
 * {@link #current} before {@code setTheme}, so the first frame is already in
 * the stored palette and there is no default-to-stored flicker. Afterwards the
 * value is process-scoped: a recreation re-reads the same object, a HOME/resume
 * touches nothing, and a genuine process death re-reads the file.
 *
 * <p><b>Not project truth.</b> Nothing here knows what a project is, and no
 * project file, {@code Uri} or path is ever involved. See {@link AppPreferences}.
 */
final class AppPreferencesStore {

    /** The file's name under the app's own private preferences directory. */
    static final String FILE_NAME = "forgeshape_preferences";

    static final String KEY_SCHEMA_VERSION = "schema_version";
    static final String KEY_PALETTE = "palette";
    static final String KEY_HANDEDNESS = "handedness";
    static final String KEY_GIZMO_VISUAL_SCALE = "gizmo_visual_scale";
    static final String KEY_GIZMO_STROKE_WEIGHT = "gizmo_stroke_weight";
    static final String KEY_TOOL_LABELS = "tool_labels";

    /** The one in-memory copy; null until first read in this process. */
    private static AppPreferences current;

    private AppPreferencesStore() {
    }

    /** The preferences in force, read from disk once per process. */
    static synchronized AppPreferences current(Context context) {
        if (current == null) {
            current = read(context);
        }
        return current;
    }

    /**
     * Records new preferences, on disk and in memory, if they differ.
     *
     * @return whether anything changed, so a caller can avoid re-applying
     *         what is already in force — recreating the Activity for a tap on
     *         the palette already worn would be a flash for nothing
     */
    static synchronized boolean update(Context context, AppPreferences next) {
        if (next == null) {
            return false;
        }
        final AppPreferences before = current(context);
        if (next.equals(before)) {
            return false;
        }
        write(context, next);
        current = next;
        return true;
    }

    /**
     * Returns to the product defaults, on disk and in memory.
     *
     * <p>For verification, and for nothing else in this stage: the Settings hub
     * offers no "reset" control, because every row already shows the default.
     */
    static synchronized void resetForVerification(Context context) {
        write(context, AppPreferences.defaults());
        current = AppPreferences.defaults();
    }

    /**
     * Forgets the in-memory copy so the next read comes from disk.
     *
     * <p>For verification: an instrumentation case cannot kill its own process,
     * so this is how it proves that what was written is what a fresh process
     * would read.
     */
    static synchronized void dropCacheForVerification() {
        current = null;
    }

    /** Whether the file exists at all; verification. */
    static boolean existsOnDisk(Context context) {
        return preferences(context).contains(KEY_SCHEMA_VERSION);
    }

    private static SharedPreferences preferences(Context context) {
        return context.getApplicationContext()
                .getSharedPreferences(FILE_NAME, Context.MODE_PRIVATE);
    }

    private static AppPreferences read(Context context) {
        final SharedPreferences prefs = preferences(context);
        // Every read is guarded: a value stored under a key with the wrong TYPE
        // (a hand-edited file, a future schema) throws a ClassCastException
        // from SharedPreferences, and the answer to that is the default for that
        // field, never a crash on launch.
        final int schema = readInt(prefs, KEY_SCHEMA_VERSION, AppPreferences.SCHEMA_VERSION);
        final String palette = readString(prefs, KEY_PALETTE);
        final String handedness = readString(prefs, KEY_HANDEDNESS);
        final float scale = readFloat(prefs, KEY_GIZMO_VISUAL_SCALE);
        final String weight = readString(prefs, KEY_GIZMO_STROKE_WEIGHT);
        final Boolean toolLabels = readBoolean(prefs, KEY_TOOL_LABELS);
        return AppPreferences.fromStored(schema, palette, handedness, scale, weight,
                toolLabels);
    }

    private static void write(Context context, AppPreferences value) {
        preferences(context).edit()
                .putInt(KEY_SCHEMA_VERSION, AppPreferences.SCHEMA_VERSION)
                .putString(KEY_PALETTE, value.palette().name())
                .putString(KEY_HANDEDNESS, value.handedness().name())
                .putFloat(KEY_GIZMO_VISUAL_SCALE, value.gizmoVisualScale())
                .putString(KEY_GIZMO_STROKE_WEIGHT, value.gizmoStrokeWeight().name())
                .putBoolean(KEY_TOOL_LABELS, value.toolLabels())
                .commit();
    }

    private static int readInt(SharedPreferences prefs, String key, int fallback) {
        try {
            return prefs.getInt(key, fallback);
        } catch (ClassCastException wrongType) {
            return fallback;
        }
    }

    private static String readString(SharedPreferences prefs, String key) {
        try {
            return prefs.getString(key, null);
        } catch (ClassCastException wrongType) {
            return null;
        }
    }

    /**
     * A stored boolean, or {@code null} when the key is absent or holds another
     * type — both of which the model reads as the product default. Asked with
     * {@code contains} first because {@code getBoolean} has no "absent" answer
     * of its own.
     */
    private static Boolean readBoolean(SharedPreferences prefs, String key) {
        try {
            return prefs.contains(key) ? Boolean.valueOf(prefs.getBoolean(key, false)) : null;
        } catch (ClassCastException wrongType) {
            return null;
        }
    }

    private static float readFloat(SharedPreferences prefs, String key) {
        try {
            return prefs.getFloat(key, Float.NaN);
        } catch (ClassCastException wrongType) {
            return Float.NaN;
        }
    }

    /**
     * Writes raw values under the real keys, bypassing every rule, so a case
     * can plant an unknown name, a NaN or an out-of-range number and prove the
     * reader's fallbacks. Verification only.
     */
    static synchronized void plantForVerification(Context context, String palette,
                                                  String handedness, float scale,
                                                  String weight, boolean toolLabels,
                                                  int schema) {
        preferences(context).edit()
                .putInt(KEY_SCHEMA_VERSION, schema)
                .putString(KEY_PALETTE, palette)
                .putString(KEY_HANDEDNESS, handedness)
                .putFloat(KEY_GIZMO_VISUAL_SCALE, scale)
                .putString(KEY_GIZMO_STROKE_WEIGHT, weight)
                .putBoolean(KEY_TOOL_LABELS, toolLabels)
                .commit();
        current = null;
    }
}
