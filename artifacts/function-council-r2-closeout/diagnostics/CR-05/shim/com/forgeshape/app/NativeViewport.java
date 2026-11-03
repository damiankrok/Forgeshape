package com.forgeshape.app;

import java.util.Arrays;

/**
 * CR-05 sandbox FAKE of the JNI surface. NOT the product's native code.
 *
 * The live project is modelled as an opaque byte string: encodeProject() returns
 * it, projectFingerprint() is FNV-1a 64 over it (so "fingerprint equal" means
 * "same semantic document", the property the real fingerprint has), loadProject()
 * replaces it and empties the undo history (CLAUDE.md: a successful load starts a
 * fresh history), closeProject() empties it. Nothing here decides any outcome the
 * audit reports: the outcomes are decided by the REAL AutosaveController,
 * ProjectCheckpoint, ProjectSlot and EditorUiState compiled unchanged beside it.
 * Constants copied from NativeViewport.java:183,193.
 */
final class NativeViewport {
    static final int PRIMITIVE_BOX = 0;
    static final int PRIMITIVE_PLANE = 5;
    static final int PROJECT_OK = 0;
    static final int PROJECT_NO_DATA = 1;

    private static byte[] document;      // null == Home (no project)
    private static int undoDepth;

    static synchronized boolean projectOpen() { return document != null; }
    static synchronized long projectFingerprint() {
        if (document == null) return 0L;
        return fnv(document);
    }
    static synchronized byte[] encodeProject() { return document == null ? null : document.clone(); }
    static synchronized int validateProject(byte[] bytes) { return (bytes == null || bytes.length == 0) ? PROJECT_NO_DATA : PROJECT_OK; }
    static synchronized int loadProject(byte[] bytes) {
        if (validateProject(bytes) != PROJECT_OK) return PROJECT_NO_DATA;
        document = bytes.clone(); undoDepth = 0; return PROJECT_OK;
    }
    static synchronized void closeProject() { document = null; undoDepth = 0; }

    // --- harness-only controls (no product equivalent name) ---
    static synchronized void fakeNewProject(String content) { document = content.getBytes(); undoDepth = 0; }
    static synchronized void fakeEdit(String suffix) { document = (new String(document) + suffix).getBytes(); undoDepth++; }
    static synchronized void fakeProcessDeath() { document = null; undoDepth = 0; }
    static synchronized int fakeUndoDepth() { return undoDepth; }
    static synchronized String fakeContent() { return document == null ? "<none>" : new String(document); }

    static long fnv(byte[] b) {
        long h = 0xCBF29CE484222325L;
        for (byte x : b) { h ^= (x & 0xff); h *= 0x100000001B3L; }
        return h;
    }
    static String describe(byte[] b) { return b == null ? "<absent>" : "\"" + new String(b) + "\""; }
    private NativeViewport() {}
    static boolean same(byte[] a, byte[] b) { return Arrays.equals(a, b); }
}
