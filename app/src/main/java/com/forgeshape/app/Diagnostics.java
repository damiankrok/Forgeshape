package com.forgeshape.app;

import android.app.ActivityManager;
import android.app.ApplicationExitInfo;
import android.content.Context;
import android.os.Build;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.List;
import java.util.Locale;

/**
 * The process-scoped diagnostic channel: what happened, kept locally, shared
 * only if the user chooses to.
 *
 * <p><b>ForgeShape sends nothing anywhere.</b> There is no endpoint, no upload,
 * no analytics identifier and no background transmission in this class or
 * anywhere near it. The only way a diagnostic leaves the device is the user
 * picking a destination through the system's own document UI — see the Share
 * Diagnostics action — which means the user has read it, or could have.
 *
 * <p>This is the half that knows about Android: {@link DiagnosticLog} is the
 * bounded ring and is free of Android types so its bounds and its redaction can
 * be argued about in the JVM suite.
 *
 * <h2>What a report contains</h2>
 *
 * <p>App and build identification, the coarse device and ABI facts that decide
 * which code ran, a failure token if there is one, what Android says about how
 * the previous process ended, and the bounded log tail.
 *
 * <p><b>Never the model.</b> No geometry, no vertex, no dimension, no `.forge`
 * bytes, no file contents, no path and no document {@code Uri}. A diagnostic
 * that carried the user's work would be one they could not safely share, and a
 * diagnostic nobody can share is not worth keeping.
 */
final class Diagnostics {

    /** The file a report is written to, inside app-private storage. */
    static final String REPORT_FILE_NAME = "diagnostics.txt";

    /** Reason tokens. Stable, greppable, and never a value. */
    static final String REASON_MANUAL = "MANUAL";
    static final String REASON_UNCAUGHT = "UNCAUGHT_EXCEPTION";
    static final String REASON_RENDER_RESTART_REQUIRED = "RENDER_RESTART_REQUIRED";

    private static final DiagnosticLog LOG = new DiagnosticLog();

    private static boolean handlerInstalled;
    private static boolean previousExitChecked;

    private Diagnostics() {
    }

    static DiagnosticLog log() {
        return LOG;
    }

    static void info(String category, String token, String detail) {
        LOG.record(System.currentTimeMillis(), DiagnosticLog.INFO, category, token, detail);
    }

    static void warn(String category, String token, String detail) {
        LOG.record(System.currentTimeMillis(), DiagnosticLog.WARN, category, token, detail);
    }

    static void error(String category, String token, String detail) {
        LOG.record(System.currentTimeMillis(), DiagnosticLog.ERROR, category, token, detail);
    }

    /**
     * Chains an uncaught-exception handler that leaves a report behind.
     *
     * <p>It writes and then <b>delegates to the handler that was already
     * there</b>, so Android's own crash reporting, the debugger and the process
     * kill all still happen exactly as they would have. A handler that swallowed
     * the throwable would turn a crash into a hang, which is worse than the
     * crash.
     *
     * <p>Installed once per process. The work it does is deliberately tiny:
     * the process is dying, and a handler that tried to do anything ambitious
     * would simply not finish.
     */
    static synchronized void installUncaughtHandler(final Context context) {
        if (handlerInstalled) {
            return;
        }
        handlerInstalled = true;
        final Context appContext = context.getApplicationContext();
        final Thread.UncaughtExceptionHandler previous =
                Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler(new Thread.UncaughtExceptionHandler() {
            @Override
            public void uncaughtException(Thread thread, Throwable throwable) {
                try {
                    // The class name and message only. A stack trace would be
                    // useful and is deliberately not in the RING — it goes in
                    // the report body below, where its size is bounded once.
                    error(DiagnosticLog.CAT_LIFECYCLE, REASON_UNCAUGHT,
                            throwable.getClass().getSimpleName());
                    writeReport(appContext, REASON_UNCAUGHT, throwable);
                } catch (Throwable ignored) {
                    // A diagnostic that throws during a crash must not replace
                    // the crash. Nothing here is worth a second failure.
                } finally {
                    if (previous != null) {
                        previous.uncaughtException(thread, throwable);
                    }
                }
            }
        });
    }

    /**
     * Records what Android says about how the previous process ended.
     *
     * <p>Version-gated and defensive on purpose. {@code
     * getHistoricalProcessExitReasons} exists only from API 30, may return
     * nothing, and describes the process rather than the cause of any particular
     * fault. So this records <b>what the platform reported</b> and claims
     * nothing beyond it: an exit reason is evidence, not a diagnosis, and
     * writing "the app crashed" when the platform said "the user swiped it away"
     * would be worse than saying nothing.
     */
    static synchronized void notePreviousExit(Context context) {
        if (previousExitChecked) {
            return;
        }
        previousExitChecked = true;
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            info(DiagnosticLog.CAT_LIFECYCLE, "PREVIOUS_EXIT_UNAVAILABLE",
                    "api" + Build.VERSION.SDK_INT);
            return;
        }
        try {
            final ActivityManager manager =
                    (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
            if (manager == null) {
                return;
            }
            final List<ApplicationExitInfo> reasons =
                    manager.getHistoricalProcessExitReasons(context.getPackageName(), 0, 1);
            if (reasons == null || reasons.isEmpty()) {
                info(DiagnosticLog.CAT_LIFECYCLE, "PREVIOUS_EXIT_NONE", null);
                return;
            }
            final ApplicationExitInfo exit = reasons.get(0);
            info(DiagnosticLog.CAT_LIFECYCLE, "PREVIOUS_EXIT",
                    "reason=" + exit.getReason() + ",status=" + exit.getStatus());
        } catch (RuntimeException error) {
            // A diagnostic that cannot be taken is not a failure of the product.
            info(DiagnosticLog.CAT_LIFECYCLE, "PREVIOUS_EXIT_UNAVAILABLE",
                    error.getClass().getSimpleName());
        }
    }

    /** Whether a report is sitting in app-private storage. */
    static boolean reportExists(Context context) {
        final File file = reportFile(context);
        return file.isFile() && file.length() > 0L;
    }

    static File reportFile(Context context) {
        return new File(context.getFilesDir(), REPORT_FILE_NAME);
    }

    /** Writes a report for a reason with no throwable behind it. */
    static boolean writeReport(Context context, String reasonToken) {
        return writeReport(context, reasonToken, null);
    }

    /**
     * Writes the report into app-private storage, replacing any previous one.
     *
     * @return whether the complete file is in place
     */
    static boolean writeReport(Context context, String reasonToken, Throwable throwable) {
        final String body = renderReport(context, reasonToken, throwable);
        FileOutputStream out = null;
        try {
            out = new FileOutputStream(reportFile(context));
            out.write(body.getBytes(StandardCharsets.UTF_8));
            out.flush();
            return true;
        } catch (IOException error) {
            return false;
        } finally {
            if (out != null) {
                try {
                    out.close();
                } catch (IOException ignored) {
                    // The outcome that matters was decided by the write.
                }
            }
        }
    }

    /**
     * The report, as text.
     *
     * <p>Assembled here rather than at the call site so there is exactly one
     * answer to "what does a ForgeShape diagnostic contain", and one place to
     * check that the answer stays free of the model.
     */
    static String renderReport(Context context, String reasonToken, Throwable throwable) {
        final StringBuilder out = new StringBuilder();
        out.append("ForgeShape diagnostic report\n");
        out.append("reason=").append(DiagnosticLog.sanitize(reasonToken, 64)).append('\n');
        out.append("generatedAtMillis=").append(System.currentTimeMillis()).append('\n');
        out.append("package=").append(context.getPackageName()).append('\n');
        appendBuildIdentity(out, context);
        // Coarse device facts, because they decide which code ran. No serial, no
        // advertising id, nothing that identifies a person or a handset.
        out.append(String.format(Locale.US, "android=%d device=%s/%s abis=%s\n",
                Build.VERSION.SDK_INT, Build.MANUFACTURER, Build.MODEL,
                joinAbis()));
        out.append("rendererLifecycle=").append(safeRendererLifecycle()).append('\n');
        if (throwable != null) {
            out.append("throwable=").append(throwable.getClass().getName()).append('\n');
            // The message and a bounded trace. A stack trace names ForgeShape's
            // own code and the platform's; it carries no project data.
            out.append("message=")
               .append(DiagnosticLog.sanitize(String.valueOf(throwable.getMessage()), 200))
               .append('\n');
            final StackTraceElement[] frames = throwable.getStackTrace();
            final int limit = Math.min(frames.length, 24);
            for (int i = 0; i < limit; i++) {
                out.append("  at ").append(frames[i].toString()).append('\n');
            }
            if (frames.length > limit) {
                out.append("  ... ").append(frames.length - limit).append(" more\n");
            }
        }
        out.append("--- log (").append(LOG.size()).append(" records, ")
           .append(LOG.droppedRecords()).append(" dropped) ---\n");
        out.append(LOG.render());
        return out.toString();
    }

    /**
     * Which build this is, read from the installed package rather than from a
     * generated constants class.
     *
     * <p>{@code BuildConfig} is not generated for this module and turning it on
     * would be a build-system change for one string. The package manager already
     * knows, and asking it costs nothing on a path that runs at most once per
     * report.
     */
    private static void appendBuildIdentity(StringBuilder out, Context context) {
        try {
            final android.content.pm.PackageInfo info = context.getPackageManager()
                    .getPackageInfo(context.getPackageName(), 0);
            out.append("versionName=").append(info.versionName).append('\n');
            out.append("versionCode=").append(
                    Build.VERSION.SDK_INT >= Build.VERSION_CODES.P
                            ? info.getLongVersionCode()
                            : (long) info.versionCode).append('\n');
        } catch (Exception error) {
            out.append("versionName=unavailable\nversionCode=unavailable\n");
        }
        final int flags = context.getApplicationInfo().flags;
        out.append("debuggable=")
           .append((flags & android.content.pm.ApplicationInfo.FLAG_DEBUGGABLE) != 0)
           .append('\n');
    }

    private static String joinAbis() {
        final String[] abis = Build.SUPPORTED_ABIS;
        if (abis == null || abis.length == 0) {
            return "unknown";
        }
        final StringBuilder out = new StringBuilder();
        for (int i = 0; i < abis.length; i++) {
            if (i > 0) {
                out.append(',');
            }
            out.append(abis[i]);
        }
        return out.toString();
    }

    /**
     * The renderer's state, or a placeholder.
     *
     * <p>Guarded because a report may be written during an uncaught exception
     * thrown before the native library finished loading, and a diagnostic that
     * crashes while reporting a crash reports nothing at all.
     */
    private static String safeRendererLifecycle() {
        try {
            return String.valueOf(NativeViewport.rendererLifecycle());
        } catch (Throwable ignored) {
            return "unavailable";
        }
    }
}
