package com.forgeshape.app;

import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import android.net.Uri;

/**
 * Moving a ForgeShape project in and out of the device's own storage, through
 * the Storage Access Framework.
 *
 * <p><b>Two kinds of traffic, and the difference matters.</b> A `.forge`
 * document is project TRANSFER: the canonical bytes an explicit Save writes,
 * readable by any compatible ForgeShape installation, and readable back by
 * ForgeShape. A `.glb` is an EXPORT: a one-way, lossy-by-design view of the
 * current geometry for another tool to look at, which ForgeShape cannot read
 * back and does not pretend to.
 *
 * <p>What is still absent, deliberately: glTF/GLB <b>import</b>, OBJ, FBX, any
 * other interchange format, and any converter or menu entry that hints at one.
 * Those are separate pipelines with their own stages.
 *
 * <p><b>The Uri stops here.</b> This class is the entire boundary: it turns a
 * {@code Uri} into a {@code byte[]} and a {@code byte[]} into a document the user
 * picked. Nothing below it — not {@link NativeViewport}, not the codec, not the
 * document — ever sees a {@code Uri}, a {@code ContentResolver}, a provider
 * authority or a filesystem path, and none of those is ever stored as project
 * truth. A project that remembered where it came from would be a project that
 * could not be opened anywhere else, which is the opposite of the point.
 *
 * <p><b>Failure is non-destructive by construction.</b> Reading produces bytes
 * or null; it cannot half-apply anything, because applying is the fail-closed
 * native load's job and it happens afterwards. Writing either produces a
 * complete document at the destination the user chose or reports that it did
 * not; the internal manual slot is not involved in either direction.
 */
final class ProjectTransfer {

    /**
     * The MIME type a new document is created with.
     *
     * <p>{@code application/octet-stream} rather than a ForgeShape-specific type
     * on purpose. A custom type would be more descriptive and would also make
     * several stock document providers refuse to create the file at all, or
     * rewrite the extension to something they recognise. The user-visible fact
     * is the {@code .forge} name, which is carried in the suggested filename and
     * survives; the bytes are the v1 codec contract either way.
     */
    static final String CREATE_MIME_TYPE = "application/octet-stream";

    /**
     * What the open picker will show.
     *
     * <p>Deliberately permissive. A `.forge` file has no registered type, so
     * providers report whatever they like for it — frequently
     * {@code application/octet-stream}, sometimes nothing at all. Filtering
     * narrowly would hide the user's own project from the picker, which is a far
     * worse failure than showing files that ForgeShape will then refuse. And it
     * WILL refuse them: everything selected here goes through the same
     * fail-closed decoder, which is the actual guard.
     */
    static final String OPEN_MIME_TYPE = "*/*";

    /** The name offered for a new document. The extension is the product's own. */
    static final String SUGGESTED_FILE_NAME = "project.forge";

    /** The registered media type for a binary glTF. */
    static final String GLB_MIME_TYPE = "model/gltf-binary";

    /** The name offered for an exported model. */
    static final String SUGGESTED_GLB_FILE_NAME = "model.glb";

    /** A ceiling on what is read into memory, applied before the first byte. */
    private static final int MAX_READABLE_BYTES = 256 * 1024 * 1024;

    private ProjectTransfer() {
    }

    /** The intent that asks the system where to put a copy of the project. */
    static Intent createDocumentIntent() {
        final Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType(CREATE_MIME_TYPE);
        intent.putExtra(Intent.EXTRA_TITLE, SUGGESTED_FILE_NAME);
        return intent;
    }

    /** The intent that asks the system which project file to read. */
    static Intent openDocumentIntent() {
        final Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType(OPEN_MIME_TYPE);
        return intent;
    }

    /**
     * The intent that asks where to put an exported GLB.
     *
     * <p>{@code model/gltf-binary} is the registered type for a binary glTF and
     * is what a capable provider should be told. Several stock document
     * providers on Android refuse or rewrite an unfamiliar type, so the
     * user-visible fact — the {@code .glb} name — is carried in EXTRA_TITLE
     * where no provider can lose it, and the bytes are the GLB contract either
     * way. See ProjectTransfer#CREATE_MIME_TYPE for the same reasoning applied
     * to `.forge`.
     */
    static Intent createGlbDocumentIntent() {
        final Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType(GLB_MIME_TYPE);
        intent.putExtra(Intent.EXTRA_TITLE, SUGGESTED_GLB_FILE_NAME);
        return intent;
    }

    /**
     * The intent that asks which `.glb` to read back for the diagnostic
     * preview (GLB-IMPORT-R0).
     *
     * <p>Permissive for the same reason the project open picker is: a provider
     * reports whatever type it likes for a `.glb`, frequently
     * {@code application/octet-stream} and sometimes nothing at all, and
     * filtering narrowly would hide the file the user just exported. The real
     * guard is the fail-closed parser every selection then goes through, which
     * refuses anything that is not the supported GLB subset by name.
     */
    static Intent openGlbDocumentIntent() {
        final Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType(OPEN_MIME_TYPE);
        intent.putExtra(Intent.EXTRA_MIME_TYPES,
                new String[]{GLB_MIME_TYPE, CREATE_MIME_TYPE, OPEN_MIME_TYPE});
        return intent;
    }

    /** The intent that asks where to put a diagnostic report. */
    static Intent createDiagnosticsIntent() {
        final Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("text/plain");
        intent.putExtra(Intent.EXTRA_TITLE, "forgeshape-diagnostics.txt");
        return intent;
    }

    /**
     * Writes bytes to a document the user chose.
     *
     * <p>Truncating on purpose: {@code "wt"} replaces the document's contents
     * rather than overwriting the front of them, so choosing an existing, larger
     * file cannot leave a valid `.forge` header in front of somebody else's
     * trailing bytes.
     *
     * @return whether the complete document was written
     */
    static boolean writeTo(Context context, Uri destination, byte[] bytes) {
        if (destination == null || bytes == null || bytes.length == 0) {
            return false;
        }
        final ContentResolver resolver = context.getContentResolver();
        OutputStream out = null;
        try {
            out = resolver.openOutputStream(destination, "wt");
            if (out == null) {
                return false;
            }
            out.write(bytes);
            out.flush();
            return true;
        } catch (IOException | SecurityException | IllegalArgumentException
                 | UnsupportedOperationException error) {
            // A provider that refuses, disappears or has no permission is an
            // ordinary outcome of handing the user a system picker. It costs the
            // live project nothing: this direction only reads it.
            Diagnostics.warn(DiagnosticLog.CAT_TRANSFER, "SAVE_COPY_FAILED",
                    error.getClass().getSimpleName());
            return false;
        } finally {
            closeQuietly(out);
        }
    }

    /**
     * Reads a document the user chose.
     *
     * @return the bytes, or null when the document could not be read or is
     *         implausibly large. Null is an ordinary answer, never an exception:
     *         picking an unreadable file must not be a crash.
     */
    static byte[] readFrom(Context context, Uri source) {
        if (source == null) {
            return null;
        }
        final ContentResolver resolver = context.getContentResolver();
        InputStream in = null;
        try {
            in = resolver.openInputStream(source);
            if (in == null) {
                return null;
            }
            final ByteArrayOutputStream buffer = new ByteArrayOutputStream();
            final byte[] chunk = new byte[64 * 1024];
            int read;
            while ((read = in.read(chunk)) > 0) {
                if (buffer.size() + read > MAX_READABLE_BYTES) {
                    // Bounded before the allocation grows, not after. A provider
                    // can hand back a stream of any length, including one that
                    // never ends.
                    Diagnostics.warn(DiagnosticLog.CAT_TRANSFER, "OPEN_FILE_TOO_LARGE",
                            "bytes>" + MAX_READABLE_BYTES);
                    return null;
                }
                buffer.write(chunk, 0, read);
            }
            return buffer.size() == 0 ? null : buffer.toByteArray();
        } catch (IOException | SecurityException | IllegalArgumentException
                 | UnsupportedOperationException error) {
            Diagnostics.warn(DiagnosticLog.CAT_TRANSFER, "OPEN_FILE_FAILED",
                    error.getClass().getSimpleName());
            return null;
        } finally {
            closeQuietly(in);
        }
    }

    private static void closeQuietly(java.io.Closeable closeable) {
        if (closeable == null) {
            return;
        }
        try {
            closeable.close();
        } catch (IOException ignored) {
            // The outcome that matters was decided by the read or the write.
        }
    }
}
