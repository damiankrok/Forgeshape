package com.forgeshape.app;

import android.content.Context;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.RandomAccessFile;

/**
 * The one durable, app-private place a ForgeShape project is kept.
 *
 * <p>This is the whole of the Android storage adapter, and it is deliberately
 * narrow. It knows a directory, a filename and how to write bytes without losing
 * the previous ones; it knows nothing about what a project IS. The meaning of
 * those bytes belongs to the platform-neutral codec below JNI, which in turn
 * knows nothing about {@code Context}, a path or a filesystem — which is what
 * makes the same file readable by a ForgeShape installation on another device.
 *
 * <p><b>One slot, not a library.</b> There is exactly one file, and Save
 * replaces it. There is no file picker, no Scoped Storage or SAF document, no
 * Save As, no project browser, no thumbnail, no autosave and no crash recovery:
 * every one of those is later work with its own design, and a slot that quietly
 * grew into a half of one would be worse than the honest single slot. What this
 * buys is the thing ForgeShape has never had — work that survives the process
 * dying.
 *
 * <p><b>Durability is the point, so the write is not naive.</b> Writing straight
 * over the live file would leave a half-written project behind if the process
 * died mid-write, and the user's previous save with it. Instead the bytes go to
 * a sibling temporary file, are forced to storage with {@code fsync}, and only
 * then replace the slot by rename. A crash before the rename leaves the previous
 * project intact; a crash after it leaves the new one intact. There is no moment
 * at which the slot holds a partial file.
 */
final class ProjectSlot {

    /**
     * The extension is the product's own, and the base name says which slot this
     * is rather than what the user called it: naming is a later feature, and a
     * file called {@code project.forge} cannot pretend otherwise.
     */
    static final String SLOT_FILE_NAME = "project.forge";

    /** Where a half-written save lives until it is complete. */
    private static final String PENDING_FILE_NAME = "project.forge.pending";

    /**
     * A ceiling on what will be read back into memory.
     *
     * <p>Not a limit on how large a project may be — it is far above anything
     * the editor produces — but a refusal to allocate an arbitrary amount for a
     * file that may have been replaced by something that is not a project at
     * all. The codec applies its own bounds afterwards; this one exists before
     * the first byte is read.
     */
    private static final long MAX_READABLE_BYTES = 256L * 1024L * 1024L;

    private ProjectSlot() {
    }

    static File slotFile(Context context) {
        return new File(context.getFilesDir(), SLOT_FILE_NAME);
    }

    /** Whether there is a project to open. Does not read or validate it. */
    static boolean exists(Context context) {
        final File file = slotFile(context);
        return file.isFile() && file.length() > 0L;
    }

    /**
     * Writes bytes to the slot, replacing whatever was there.
     *
     * @return true only when the complete file is durably in place
     */
    static boolean write(Context context, byte[] bytes) {
        if (bytes == null || bytes.length == 0) {
            return false;
        }
        final File pending = new File(context.getFilesDir(), PENDING_FILE_NAME);
        final File slot = slotFile(context);
        FileOutputStream out = null;
        // Set only on the one path where the pending file is the last copy of
        // the project that exists. See the comment where it is raised.
        boolean pendingIsTheOnlyCopy = false;
        try {
            out = new FileOutputStream(pending);
            out.write(bytes);
            out.flush();
            // The bytes are in the page cache after write(); this is what puts
            // them on storage. Without it the rename below could be durable
            // while its contents are not — which is exactly the failure this
            // whole dance exists to prevent.
            out.getFD().sync();
            out.close();
            out = null;
            if (pending.renameTo(slot)) {
                return true;
            }
            // renameTo cannot replace an existing file on every filesystem
            // Android has shipped, so the slot is removed and the rename
            // retried. That opens a window in which there is no slot at all —
            // and if the retry then fails, this complete, fsync'd pending file
            // is the ONLY copy of the project left. Deleting it in the finally
            // below would turn a failed save into total loss, so it is kept: a
            // stale pending file costs one file's worth of storage and is
            // overwritten by the next save, which is the cheaper of the two
            // outcomes by a wide margin.
            if (slot.delete() && pending.renameTo(slot)) {
                return true;
            }
            pendingIsTheOnlyCopy = !slot.exists();
            return false;
        } catch (IOException error) {
            return false;
        } finally {
            closeQuietly(out);
            // A pending file that never became the slot is otherwise a
            // half-written project and is worth nothing to anybody.
            if (!pendingIsTheOnlyCopy && pending.exists()) {
                pending.delete();
            }
        }
    }

    /**
     * Reads the slot.
     *
     * @return the bytes, or null when there is nothing to read, the file is
     *         unreadable, or it is implausibly large. A null answer is a normal
     *         outcome and never an exception: opening a project that is not
     *         there must not be a crash.
     */
    static byte[] read(Context context) {
        final File slot = slotFile(context);
        if (!slot.isFile()) {
            return null;
        }
        final long length = slot.length();
        if (length <= 0L || length > MAX_READABLE_BYTES) {
            return null;
        }
        RandomAccessFile file = null;
        try {
            file = new RandomAccessFile(slot, "r");
            final byte[] bytes = new byte[(int) length];
            file.readFully(bytes);
            return bytes;
        } catch (IOException error) {
            return null;
        } finally {
            closeQuietly(file);
        }
    }

    private static void closeQuietly(java.io.Closeable closeable) {
        if (closeable == null) {
            return;
        }
        try {
            closeable.close();
        } catch (IOException ignored) {
            // Nothing useful to do, and nothing the caller can act on: the
            // outcome that matters was decided by the write and the rename.
        }
    }
}
