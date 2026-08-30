package com.forgeshape.app;

import android.content.Context;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.SystemClock;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Decides <b>when</b> the project is checkpointed, and does it off the UI thread.
 *
 * <p>The problem this solves is not "write a file". It is that a modelling app
 * changes its project hundreds of times a second while a finger is down, and a
 * checkpoint per change would be a write storm that made sculpting stutter and
 * wore out storage for no benefit — while a checkpoint that never happened would
 * lose the work it exists to protect.
 *
 * <h2>The policy, in four rules</h2>
 *
 * <ol>
 *   <li><b>Nothing is written unless the project actually changed.</b> Change is
 *       decided by {@link NativeViewport#projectFingerprint()}, which hashes what
 *       the file would contain rather than counting calls, so a rejected Apply, a
 *       no-op edit and a camera orbit are all correctly nothing.</li>
 *   <li><b>Dirty generations coalesce.</b> A note only schedules; it never
 *       writes. Every note replaces the pending one, so a hundred notes during a
 *       stroke produce one checkpoint of the newest state, not a hundred
 *       checkpoints of intermediate ones.</li>
 *   <li><b>Serialization happens on a worker thread.</b> Encoding takes the
 *       native state lock, and taking it on the UI thread would let a large
 *       project's encode land inside a frame.</li>
 *   <li><b>Backgrounding flushes immediately.</b> A process that is about to be
 *       killed has no time for a debounce, so leaving the foreground asks for the
 *       latest generation right away instead of waiting out the delay.</li>
 * </ol>
 *
 * <h2>The debounce is an implementation detail, not product semantics</h2>
 *
 * <p>{@link #DEBOUNCE_MILLIS} is chosen to sit above the gap between two edits
 * inside one gesture and below the gap a user leaves between two thoughts. It is
 * not a promise, nothing in the product is described in terms of it, and it may
 * change. <b>No test waits it out</b>: tests use {@link #awaitIdle}, which is a
 * real barrier through the worker thread rather than a bet on wall-clock luck.
 *
 * <p><b>What is never checkpointed:</b> undo stacks, renderer caches, UI
 * geometry, the camera, panel state or anything else device-local. A checkpoint
 * is the same canonical `.forge` document an explicit Save writes — see
 * {@link ProjectCheckpoint}.
 */
final class AutosaveController {

    /**
     * How long a change waits for a quieter moment before being checkpointed.
     *
     * <p>An implementation detail. See the class comment: it is not product
     * semantics and no test depends on its value.
     */
    static final long DEBOUNCE_MILLIS = 900L;

    private final Context context;
    private final HandlerThread thread;
    private final Handler worker;

    /**
     * The fingerprint the checkpoint on disk was written from.
     *
     * <p>Worker-thread only. It is what makes a second checkpoint of unchanged
     * work free rather than merely fast.
     */
    private long checkpointedFingerprint;

    /** Whether a checkpoint has ever been written by this controller. */
    private boolean everCheckpointed;

    // Counters. Introspection for the tests and the diagnostic log, and the
    // evidence that the coalescing rule is doing what it claims.
    private int requestCount;
    private int writeCount;
    private int skippedUnchangedCount;
    private int failedWriteCount;

    private boolean released;

    AutosaveController(Context context) {
        this.context = context.getApplicationContext();
        this.thread = new HandlerThread("forgeshape-autosave");
        this.thread.start();
        this.worker = new Handler(thread.getLooper());
    }

    /**
     * Tells the controller that the project MAY have changed.
     *
     * <p>Cheap and safe to call often — after every native mutation, at the end
     * of every gesture, on every lifecycle edge. It schedules; it never writes,
     * and it never asks native code anything on the calling thread.
     */
    void noteMaybeDirty() {
        schedule(DEBOUNCE_MILLIS);
    }

    /**
     * Asks for a checkpoint of the latest generation as soon as possible.
     *
     * <p>For the moments where waiting is not an option: the Activity is
     * stopping, the renderer has died, the process may be about to end. Still
     * coalescing — an immediate request replaces a pending debounced one rather
     * than queueing behind it.
     */
    void requestImmediateCheckpoint() {
        schedule(0L);
    }

    private synchronized void schedule(long delayMillis) {
        if (released) {
            return;
        }
        requestCount++;
        // Replace rather than add: this is the coalescing rule, and it is one
        // line because the handler already owns the queue. A newer generation
        // supersedes an older queued one by construction — the runnable reads
        // the fingerprint when it RUNS, not when it was posted.
        worker.removeCallbacks(checkpointTask);
        worker.postDelayed(checkpointTask, delayMillis);
    }

    private final Runnable checkpointTask = new Runnable() {
        @Override
        public void run() {
            performCheckpoint();
        }
    };

    /**
     * The whole of the write, on the worker thread.
     *
     * <p>Reading the fingerprint here rather than at schedule time is what makes
     * "newest wins" true: whatever the project is at the moment the task runs is
     * what gets written, and everything that happened between the note and now
     * is already included.
     */
    private void performCheckpoint() {
        final long fingerprint;
        try {
            fingerprint = NativeViewport.projectFingerprint();
        } catch (Throwable error) {
            // Native not ready, or gone. A checkpoint that cannot be taken is
            // not a crash.
            Diagnostics.warn(DiagnosticLog.CAT_PERSISTENCE, "AUTOSAVE_FINGERPRINT_UNAVAILABLE",
                    error.getClass().getSimpleName());
            return;
        }
        if (everCheckpointed && fingerprint == checkpointedFingerprint) {
            synchronized (this) {
                skippedUnchangedCount++;
            }
            return;
        }

        final long startedAt = SystemClock.uptimeMillis();
        final byte[] bytes = NativeViewport.encodeProject();
        if (bytes == null || bytes.length == 0) {
            synchronized (this) {
                failedWriteCount++;
            }
            Diagnostics.warn(DiagnosticLog.CAT_PERSISTENCE, "AUTOSAVE_ENCODE_FAILED", null);
            return;
        }
        if (!ProjectCheckpoint.write(context, bytes)) {
            synchronized (this) {
                failedWriteCount++;
            }
            // The previous checkpoint, if any, is still on disk and still valid:
            // ProjectCheckpoint never destroys a good one to fail at a new one.
            Diagnostics.warn(DiagnosticLog.CAT_PERSISTENCE, "AUTOSAVE_WRITE_FAILED",
                    "bytes=" + bytes.length);
            return;
        }
        checkpointedFingerprint = fingerprint;
        everCheckpointed = true;
        synchronized (this) {
            writeCount++;
        }
        Diagnostics.info(DiagnosticLog.CAT_PERSISTENCE, "AUTOSAVE_CHECKPOINT",
                "bytes=" + bytes.length + ",ms=" + (SystemClock.uptimeMillis() - startedAt));
    }

    /**
     * Tells the controller the project on disk and the project in memory now
     * agree, so the next unchanged checkpoint is free.
     *
     * <p>Called after an explicit Save, a successful Open and a successful
     * Recover — every act that makes the live project something the user already
     * has safely stored. Without it the first autosave after a Save would
     * rewrite the identical document for nothing.
     */
    void noteProjectPersisted() {
        worker.post(new Runnable() {
            @Override
            public void run() {
                try {
                    checkpointedFingerprint = NativeViewport.projectFingerprint();
                    everCheckpointed = true;
                } catch (Throwable ignored) {
                    // Leaving it unset only costs one redundant checkpoint.
                }
            }
        });
    }

    /**
     * Blocks until every scheduled checkpoint has been performed.
     *
     * <p>The test hook, and the reason no test in this stage sleeps for
     * wall-clock luck. It runs any pending task <b>now</b> and then waits behind
     * it on the same single worker thread, so when it returns the disk really
     * does reflect the last note.
     *
     * @return true when the queue drained within the timeout
     */
    boolean awaitIdle(long timeoutMillis) {
        synchronized (this) {
            if (released) {
                return true;
            }
            // Pull any debounced task forward: a test asking for idle is asking
            // for the work to be done, not for the delay to elapse.
            worker.removeCallbacks(checkpointTask);
            worker.post(checkpointTask);
        }
        final CountDownLatch latch = new CountDownLatch(1);
        if (!worker.post(new Runnable() {
            @Override
            public void run() {
                latch.countDown();
            }
        })) {
            return false;
        }
        try {
            return latch.await(timeoutMillis, TimeUnit.MILLISECONDS);
        } catch (InterruptedException error) {
            Thread.currentThread().interrupt();
            return false;
        }
    }

    /**
     * Stops accepting new work and lets the worker thread finish what it has.
     *
     * <p>Deliberately does <b>not</b> cancel the pending task. {@code
     * quitSafely} runs what is already queued and then exits, and what is
     * already queued at this point is very often the immediate checkpoint that
     * {@code onStop} asked for a moment ago — the single most important write
     * this class ever performs, because it is the one that runs when the process
     * may be about to end. Removing it here to be tidy would throw away exactly
     * the work autosave exists to protect.
     */
    synchronized void release() {
        if (released) {
            return;
        }
        released = true;
        thread.quitSafely();
    }

    // --- introspection, for tests and the diagnostic log ---------------------

    synchronized int requestCount() {
        return requestCount;
    }

    /** How many checkpoints were actually written to disk. */
    synchronized int writeCount() {
        return writeCount;
    }

    /** How many scheduled checkpoints found nothing to do. */
    synchronized int skippedUnchangedCount() {
        return skippedUnchangedCount;
    }

    synchronized int failedWriteCount() {
        return failedWriteCount;
    }
}
