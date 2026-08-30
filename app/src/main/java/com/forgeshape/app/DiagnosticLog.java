package com.forgeshape.app;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Deque;
import java.util.List;
import java.util.Locale;

/**
 * A small, bounded, local record of what happened to the user's project.
 *
 * <p><b>Local means local.</b> ForgeShape sends nothing anywhere. This is a ring
 * in memory that the user can choose to write to a file they pick, and that is
 * the entire distribution mechanism. There is no endpoint, no upload, no
 * analytics identifier and no background transmission — see
 * {@code EditorWorkspaceView}'s Share Diagnostics path, which is an ordinary
 * document-creation intent.
 *
 * <p><b>Deliberately free of Android types.</b> A ring buffer with a cap is
 * arithmetic, and arithmetic should be arguable without a device — so this class
 * is plain Java and its bounds and redaction are covered by the JVM suite.
 * {@link DiagnosticReport} is the part that knows about {@code Context} and
 * build fields, and it is a separate file for exactly that reason.
 *
 * <h2>What may be recorded, and what may never be</h2>
 *
 * <p>Records are <b>tokens</b>: a category, a stable event name and a short
 * bounded detail. What that buys is a log a user can read before deciding to
 * share it.
 *
 * <p>Never recorded: geometry of any kind, a vertex, a dimension, `.forge`
 * bytes, a file's contents, a filesystem path, a document {@code Uri}, or any
 * credential. The detail field is truncated at {@link #MAX_DETAIL_CHARS}, which
 * is short enough that no payload fits through it even by accident, and
 * {@link #sanitize} strips the separators and newlines that would otherwise let
 * one record forge another.
 *
 * <p>The one thing worth stating plainly: a diagnostic that carried the model
 * would be a diagnostic nobody could safely share, and a diagnostic nobody can
 * share is not worth keeping.
 */
final class DiagnosticLog {

    /** Severity, ordered by how much a reader should care. */
    static final int INFO = 0;
    static final int WARN = 1;
    static final int ERROR = 2;

    /**
     * How many records are kept.
     *
     * <p>Bounded because an unbounded log on a device is a slow memory leak and
     * an unbounded report is one nobody reads. Two hundred is enough to hold a
     * whole session's persistence and lifecycle history several times over —
     * these are user-act-scale events, not per-frame ones.
     */
    static final int MAX_RECORDS = 200;

    /** The longest detail a single record may carry. */
    static final int MAX_DETAIL_CHARS = 120;

    /** A hard ceiling on the rendered report, whatever the records say. */
    static final int MAX_RENDERED_CHARS = 64 * 1024;

    // Categories. Stable strings, because a report is read by a human and
    // grepped by a script, and both want the same word every time.
    static final String CAT_PERSISTENCE = "persistence";
    static final String CAT_RECOVERY = "recovery";
    static final String CAT_LIFECYCLE = "lifecycle";
    static final String CAT_RENDER = "render";
    static final String CAT_TRANSFER = "transfer";

    /** One line of the log. Immutable. */
    static final class Record {
        final long timestampMillis;
        final int severity;
        final String category;
        final String token;
        final String detail;

        Record(long timestampMillis, int severity, String category, String token, String detail) {
            this.timestampMillis = timestampMillis;
            this.severity = severity;
            this.category = category;
            this.token = token;
            this.detail = detail;
        }

        /** One record, in the one format the report and the tests both read. */
        String render() {
            return String.format(Locale.US, "%d %s %s %s %s", timestampMillis,
                    severityName(severity), category, token, detail);
        }
    }

    private final Deque<Record> records = new ArrayDeque<>();

    /**
     * Records dropped because the ring was full.
     *
     * <p>Reported in the rendered log rather than hidden: a bounded log that
     * silently discards its own beginning invites a reader to conclude that
     * nothing happened before the oldest line.
     */
    private int droppedRecords;

    DiagnosticLog() {
    }

    static String severityName(int severity) {
        switch (severity) {
            case ERROR:
                return "ERROR";
            case WARN:
                return "WARN";
            case INFO:
            default:
                return "INFO";
        }
    }

    /**
     * Adds one record, dropping the oldest when the ring is full.
     *
     * @param token  a stable event name, never a value
     * @param detail short bounded context, truncated and sanitized; may be null
     */
    synchronized void record(long timestampMillis, int severity, String category, String token,
                             String detail) {
        final Record entry = new Record(timestampMillis, severity, sanitize(category, 32),
                sanitize(token, 64), sanitize(detail, MAX_DETAIL_CHARS));
        records.addLast(entry);
        while (records.size() > MAX_RECORDS) {
            records.removeFirst();
            droppedRecords++;
        }
    }

    /**
     * Strips what would let a record lie, then truncates.
     *
     * <p>Newlines and the space separator are removed rather than escaped: a
     * detail containing a newline could otherwise render as two records, one of
     * them fabricated. Truncation is what keeps a payload from fitting through
     * the field at all — the cap is far below any geometry, any `.forge` file
     * and any path worth hiding.
     */
    static String sanitize(String value, int maxChars) {
        if (value == null || value.isEmpty()) {
            return "-";
        }
        final StringBuilder out = new StringBuilder(Math.min(value.length(), maxChars));
        for (int i = 0; i < value.length() && out.length() < maxChars; i++) {
            final char c = value.charAt(i);
            if (c == '\n' || c == '\r' || c == ' ' || c == '\t') {
                out.append('_');
            } else if (c < 0x20 || c == 0x7f) {
                out.append('.');
            } else {
                out.append(c);
            }
        }
        if (out.length() == 0) {
            return "-";
        }
        if (value.length() > maxChars) {
            out.append('~');  // says plainly that something was cut
        }
        return out.toString();
    }

    synchronized int size() {
        return records.size();
    }

    synchronized int droppedRecords() {
        return droppedRecords;
    }

    synchronized List<Record> snapshot() {
        return new ArrayList<>(records);
    }

    synchronized void clear() {
        records.clear();
        droppedRecords = 0;
    }

    /**
     * The whole ring as text, newest last, capped.
     *
     * <p>The cap is applied to the OUTPUT as well as to the record count,
     * because both are ways a report could grow past what a person will read or
     * a share sheet will carry.
     */
    synchronized String render() {
        final StringBuilder out = new StringBuilder();
        if (droppedRecords > 0) {
            out.append("... ").append(droppedRecords)
               .append(" older record(s) dropped by the ring bound\n");
        }
        for (Record record : records) {
            if (out.length() + record.render().length() + 1 > MAX_RENDERED_CHARS) {
                out.append("... truncated at the rendered-size bound\n");
                break;
            }
            out.append(record.render()).append('\n');
        }
        return out.toString();
    }
}
