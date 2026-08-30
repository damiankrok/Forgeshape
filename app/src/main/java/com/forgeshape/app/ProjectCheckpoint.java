package com.forgeshape.app;

import android.content.Context;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.RandomAccessFile;

/**
 * The recovery checkpoint: a <b>separate</b> app-private file that autosave
 * writes, so that work interrupted before an explicit Save is not lost.
 *
 * <p><b>It is not the manual slot, and that separation is the point.</b>
 * {@link ProjectSlot} is what the user chose to keep; this is what the app
 * noticed they had done. Writing autosave into the manual slot would mean every
 * stroke silently overwrote the last thing the user deliberately saved, turning
 * a safety net into a destroyer of the one file they trusted. So there are two
 * files, they are written by different acts, and only the user's Save touches
 * the one the user named.
 *
 * <p>The bytes are the same canonical `.forge` v1 document the codec produces
 * for an explicit Save. There is no second format, no delta, no journal and no
 * private encoding: a checkpoint is a project file that happens to have been
 * written automatically, which is why the ordinary fail-closed decoder can read
 * it and the ordinary load path can apply it.
 *
 * <p><b>The write is atomic, and a failed one is harmless.</b> Bytes go to a
 * sibling temporary file, are forced to storage, and only then replace the
 * checkpoint by rename. A crash before the rename leaves the previous valid
 * checkpoint exactly where it was; a crash after it leaves the new one. There is
 * no moment at which the checkpoint file holds a partial document — which
 * matters far more here than for a manual save, because this file is written
 * while the user is working and is read after something went wrong.
 *
 * <p>A checkpoint that could not be read is <b>quarantined</b> rather than
 * retried. See {@link #quarantine}: a corrupt candidate that stayed a candidate
 * would offer the same broken recovery on every single launch, which is worse
 * than losing it once and saying so.
 */
final class ProjectCheckpoint {

    /** The recovery file. Named for what it is, beside the manual slot. */
    static final String CHECKPOINT_FILE_NAME = "recovery.forge";

    /** Where a half-written checkpoint lives until it is complete. */
    private static final String PENDING_FILE_NAME = "recovery.forge.pending";

    /**
     * Where a checkpoint goes when it turns out not to be readable.
     *
     * <p>Kept rather than deleted: it is the only evidence of what went wrong,
     * it is bounded to one file, and it is replaced by the next quarantine. It
     * is never offered as a candidate again.
     */
    private static final String QUARANTINE_FILE_NAME = "recovery.forge.quarantine";

    /** The same ceiling the manual slot applies, and for the same reason. */
    private static final long MAX_READABLE_BYTES = 256L * 1024L * 1024L;

    private ProjectCheckpoint() {
    }

    static File checkpointFile(Context context) {
        return new File(context.getFilesDir(), CHECKPOINT_FILE_NAME);
    }

    static File quarantineFile(Context context) {
        return new File(context.getFilesDir(), QUARANTINE_FILE_NAME);
    }

    /** Whether a checkpoint file is present. Says nothing about whether it decodes. */
    static boolean exists(Context context) {
        final File file = checkpointFile(context);
        return file.isFile() && file.length() > 0L;
    }

    /**
     * Replaces the checkpoint with these bytes.
     *
     * <p>A false answer means the previous checkpoint — if there was one — is
     * still intact and still valid. Autosave never destroys a good checkpoint in
     * order to fail at writing a new one.
     */
    static boolean write(Context context, byte[] bytes) {
        if (bytes == null || bytes.length == 0) {
            return false;
        }
        final File pending = new File(context.getFilesDir(), PENDING_FILE_NAME);
        final File checkpoint = checkpointFile(context);
        FileOutputStream out = null;
        boolean pendingIsTheOnlyCopy = false;
        try {
            out = new FileOutputStream(pending);
            out.write(bytes);
            out.flush();
            // The bytes are only in the page cache until this returns. Without
            // it the rename could be durable while the contents were not, which
            // is precisely the corrupt-checkpoint case this class exists to
            // make impossible.
            out.getFD().sync();
            out.close();
            out = null;
            if (pending.renameTo(checkpoint)) {
                return true;
            }
            // renameTo cannot replace an existing file on every filesystem
            // Android has shipped. Deleting first opens a window with no
            // checkpoint at all; if the retry then fails, this complete,
            // fsync'd pending file is the only copy of the work that exists, so
            // it is kept rather than tidied away.
            if (checkpoint.delete() && pending.renameTo(checkpoint)) {
                return true;
            }
            pendingIsTheOnlyCopy = !checkpoint.exists();
            return false;
        } catch (IOException error) {
            return false;
        } finally {
            closeQuietly(out);
            if (!pendingIsTheOnlyCopy && pending.exists()) {
                pending.delete();
            }
        }
    }

    /** The checkpoint bytes, or null when there is nothing readable to return. */
    static byte[] read(Context context) {
        final File checkpoint = checkpointFile(context);
        if (!checkpoint.isFile()) {
            return null;
        }
        final long length = checkpoint.length();
        if (length <= 0L || length > MAX_READABLE_BYTES) {
            return null;
        }
        RandomAccessFile file = null;
        try {
            file = new RandomAccessFile(checkpoint, "r");
            final byte[] bytes = new byte[(int) length];
            file.readFully(bytes);
            return bytes;
        } catch (IOException error) {
            return null;
        } finally {
            closeQuietly(file);
        }
    }

    /**
     * Retires the checkpoint because the work it protected is now safe.
     *
     * <p>Called after an explicit Save (the same work is in the manual slot),
     * after a successful Recover or Open (it is the live project), and after a
     * Discard (the user said so). Autosave writes a fresh one the moment the
     * project changes again.
     */
    static boolean clear(Context context) {
        final File checkpoint = checkpointFile(context);
        return !checkpoint.exists() || checkpoint.delete();
    }

    /**
     * Moves an unreadable checkpoint out of the candidate position, for good.
     *
     * <p>This is what stops a corrupt recovery from asking the same broken
     * question on every launch forever. The bytes are kept in one bounded
     * quarantine file rather than deleted, because they are the only evidence of
     * what happened, but nothing ever looks at them again as a candidate.
     */
    static boolean quarantine(Context context) {
        final File checkpoint = checkpointFile(context);
        if (!checkpoint.isFile()) {
            return false;
        }
        final File quarantine = quarantineFile(context);
        if (quarantine.exists() && !quarantine.delete()) {
            // Cannot park it: losing the candidate outright is still better than
            // offering an unreadable one on every launch.
            return checkpoint.delete();
        }
        if (checkpoint.renameTo(quarantine)) {
            return true;
        }
        return checkpoint.delete();
    }

    private static void closeQuietly(java.io.Closeable closeable) {
        if (closeable == null) {
            return;
        }
        try {
            closeable.close();
        } catch (IOException ignored) {
            // Nothing useful to do; the outcome was decided by the write.
        }
    }
}
