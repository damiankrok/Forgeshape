package com.forgeshape.app;

import android.content.Context;

import java.io.File;
import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.nio.file.Files;

/**
 * CR-05 host reproduction. Every file lives in a fresh temp directory under the
 * scratch root given as args[0]; nothing outside it is read or written.
 * Lifecycle edges are modelled on ForgeShapeActivity.java:
 *   recreate (palette)  = requestTheme :268-280 -> onStop :353-359 -> onDestroy :569-584 -> onCreate (new workspace)
 *   background          = onStop :353-359 (immediate checkpoint), then the worker drains
 *   process death       = background, then the process (native scene, statics, threads) is gone; cold launch
 */
public final class Cr05Repro {
    private static File scratchRoot;
    private static Context ctx;
    private static WorkspaceTranscript ws;
    private static int step;

    public static void main(String[] args) throws Exception {
        scratchRoot = new File(args[0]);
        scratchRoot.mkdirs();
        a01();
        a02();
        a02ControlOpenFile();
        a02ControlEditAfterRecover();
        a05();
        a05ControlNoRecreate();
        e02();
        e02ControlAnsweredCandidate();
        e02SavedWorkReoffered();
        a06TranscriptOnly();
        uiStateFields();
        System.out.println("CR05_HARNESS_DONE");
    }

    // ---------------------------------------------------------------- helpers
    private static void freshProcess(String name) throws Exception {
        final File dir = Files.createTempDirectory(scratchRoot.toPath(), name + "-").toFile();
        ctx = new Context(dir);
        NativeViewport.fakeProcessDeath();
        new EditorUiState().clearRecoveryResolved();   // statics start false in a new process
        EditorUiState.forNewWorkspace();               // and nothing is carried
        step = 0;
        System.out.println();
        System.out.println("=== " + name);
        ws = new WorkspaceTranscript(ctx);             // ForgeShapeActivity.onCreate -> workspace ctor
        row("cold launch (workspace constructor)");
    }
    private static void idle() { ws.autosave.awaitIdle(5000); }
    private static void edit(String s) { NativeViewport.fakeEdit(s); ws.noteProjectMaybeDirty(); }
    private static void background() { ws.requestImmediateCheckpoint(); idle(); }
    private static void recreate() {
        EditorUiState.carryAcrossRecreation(ws.uiState); // requestTheme :277
        ws.requestImmediateCheckpoint();                  // onStop :358
        idle();                                           // deterministic: the old worker's queued write lands first
        ws.releaseAutosave();                             // onDestroy :578
        ws = new WorkspaceTranscript(ctx);                // onCreate :51 -> new workspace
    }
    private static void processDeathAndColdLaunch() {
        background();
        ws.releaseAutosave();
        NativeViewport.fakeProcessDeath();
        new EditorUiState().clearRecoveryResolved();
        EditorUiState.forNewWorkspace();
        ws = new WorkspaceTranscript(ctx);
    }
    private static String file(byte[] b) { return b == null ? "absent" : "\"" + new String(b) + "\""; }
    private static void row(String what) {
        System.out.printf("  %02d %-50s live=%-8s slot=%-8s ckpt=%-8s dirty=%-5s everPersisted=%-5s resolved=%-5s recoveryPrompt=%-5s unsavedPrompt=%-5s undo=%d%n",
                ++step, what, NativeViewport.projectOpen() ? "\"" + NativeViewport.fakeContent() + "\"" : "<home>",
                file(ProjectSlot.read(ctx)), file(ProjectCheckpoint.read(ctx)),
                ws.projectDirty(), ws.everPersisted, ws.uiState.recoveryResolved(),
                ws.recoveryPromptVisible, ws.unsavedPromptVisible, NativeViewport.fakeUndoDepth());
    }
    private static void verdict(String token, boolean holds) {
        System.out.println("  " + token + "=" + (holds ? "OBSERVED" : "NOT_OBSERVED"));
    }

    // ---------------------------------------------------------------- A-01
    private static void a01() throws Exception {
        freshProcess("A01_save_and_continue_before_open_saved");
        ws.uiState.recordRecoveryResolved();
        NativeViewport.fakeNewProject("A"); ws.noteProjectUnpersisted(); row("new project A (unpersisted)");
        ws.onSaveProjectRequested(); row("Save Project");
        edit("+b"); row("edit -> A+b"); idle(); row("autosave debounce drained");
        ws.onOpenProjectRequested(); row("drawer: Open Saved Project");
        ws.onUnsavedSaveRequested(); row("unsaved question: Save and continue");
        final byte[] slot = ProjectSlot.read(ctx);
        verdict("A01_SLOT_NO_LONGER_HOLDS_A", slot != null && !"A".equals(new String(slot)));
        verdict("A01_OPENED_PROJECT_IS_THE_LIVE_ONE_NOT_THE_SAVED_ONE", "A+b".equals(NativeViewport.fakeContent()));
        verdict("A01_UNDO_HISTORY_EMPTIED_BY_THE_RELOAD", NativeViewport.fakeUndoDepth() == 0);
        verdict("A01_A_EXISTS_IN_NO_FILE", slot != null && !"A".equals(new String(slot))
                && (ProjectCheckpoint.read(ctx) == null || !"A".equals(new String(ProjectCheckpoint.read(ctx)))));
    }

    // ---------------------------------------------------------------- A-02
    private static void a02() throws Exception {
        freshProcess("A02_recovered_work_protected_nowhere");
        ws.uiState.recordRecoveryResolved();
        NativeViewport.fakeNewProject("W"); ws.noteProjectUnpersisted(); edit("+1"); row("new project, edit -> W+1 (never saved)");
        idle(); row("autosave debounce drained");
        processDeathAndColdLaunch(); row("background + process death + cold launch");
        ws.onRecoverRequested(); row("Recover");
        background(); row("background (onStop immediate checkpoint drained)");
        verdict("A02_NO_CHECKPOINT_AFTER_RECOVER_AND_BACKGROUND", ProjectCheckpoint.read(ctx) == null);
        verdict("A02_NOT_IN_SLOT", ProjectSlot.read(ctx) == null);
        verdict("A02_NOT_DIRTY_SO_LEAVING_ASKS_NOTHING", !ws.projectDirty());
        System.out.println("  autosave counters: writes=" + ws.autosave.writeCount()
                + " skippedUnchanged=" + ws.autosave.skippedUnchangedCount());
        processDeathAndColdLaunch(); row("second process death + cold launch");
        verdict("A02_RECOVERED_WORK_LOST_ON_SECOND_DEATH", !ws.recoveryPromptVisible
                && ProjectSlot.read(ctx) == null && ProjectCheckpoint.read(ctx) == null);
        // the 'leave' variant
        freshProcess("A02b_recover_then_new_project");
        ws.uiState.recordRecoveryResolved();
        NativeViewport.fakeNewProject("W"); ws.noteProjectUnpersisted(); edit("+1"); idle();
        processDeathAndColdLaunch(); row("cold launch with candidate W+1");
        ws.onRecoverRequested(); row("Recover");
        ws.leaveProjectFor(WorkspaceTranscript.LEAVE_NEW_PROJECT); row("drawer: New Project...");
        verdict("A02b_NEW_PROJECT_ASKED_NOTHING_AND_CLOSED_RECOVERED_WORK", !ws.unsavedPromptVisible
                && !NativeViewport.projectOpen() && ProjectCheckpoint.read(ctx) == null && ProjectSlot.read(ctx) == null);
    }
    private static void a02ControlOpenFile() throws Exception {
        freshProcess("A02_control_open_file_keeps_protection");
        ws.uiState.recordRecoveryResolved();
        NativeViewport.fakeNewProject("X"); ws.noteProjectUnpersisted();
        ws.openFileApplied("F".getBytes()); row("Open File... applied a document F");
        background(); row("background");
        verdict("CONTROL_OPEN_FILE_IS_CHECKPOINTED", ProjectCheckpoint.read(ctx) != null);
    }
    private static void a02ControlEditAfterRecover() throws Exception {
        freshProcess("A02_control_edit_after_recover");
        ws.uiState.recordRecoveryResolved();
        NativeViewport.fakeNewProject("W"); ws.noteProjectUnpersisted(); edit("+1"); idle();
        processDeathAndColdLaunch();
        ws.onRecoverRequested(); row("Recover");
        edit("+2"); background(); row("edit -> W+1+2, background");
        verdict("CONTROL_ONE_EDIT_AFTER_RECOVER_RESTORES_A_CHECKPOINT", ProjectCheckpoint.read(ctx) != null);
    }

    // ---------------------------------------------------------------- A-05 / E-04
    private static void a05() throws Exception {
        freshProcess("A05_saved_baseline_lost_on_recreation");
        ws.uiState.recordRecoveryResolved();
        NativeViewport.fakeNewProject("S"); ws.noteProjectUnpersisted(); row("new project S");
        ws.onSaveProjectRequested(); row("Save Project");
        final boolean dirtyBefore = ws.projectDirty();
        final long fpBefore = NativeViewport.projectFingerprint();
        recreate(); row("Settings: choose another palette (recreate)");
        verdict("A05_FINGERPRINT_UNCHANGED", fpBefore == NativeViewport.projectFingerprint());
        verdict("A05_DIRTY_FLIPPED_FALSE_TO_TRUE", !dirtyBefore && ws.projectDirty());
        ws.leaveProjectFor(WorkspaceTranscript.LEAVE_NEW_PROJECT); row("drawer: New Project...");
        verdict("A05_UNSAVED_QUESTION_OVER_A_SAVED_PROJECT", ws.unsavedPromptVisible);
    }
    private static void a05ControlNoRecreate() throws Exception {
        freshProcess("A05_control_no_recreation");
        ws.uiState.recordRecoveryResolved();
        NativeViewport.fakeNewProject("S"); ws.noteProjectUnpersisted();
        ws.onSaveProjectRequested(); row("Save Project");
        ws.leaveProjectFor(WorkspaceTranscript.LEAVE_NEW_PROJECT); row("drawer: New Project...");
        verdict("CONTROL_NO_QUESTION_WITHOUT_RECREATION", !ws.unsavedPromptVisible);
    }

    // ---------------------------------------------------------------- E-02
    private static void e02() throws Exception {
        freshProcess("E02_recovery_question_over_live_project");
        // no candidate at cold launch -> offerRecoveryIfPresent returns false early, resolved stays false
        NativeViewport.fakeNewProject("L"); ws.noteProjectUnpersisted(); row("New Project (no recovery question ever asked)");
        edit("+e"); idle(); row("edit -> L+e, autosave debounce drained");
        recreate(); row("Settings: choose another palette (recreate)");
        verdict("E02_RECOVERY_PROMPT_SHOWN_OVER_OPEN_PROJECT", ws.recoveryPromptVisible && NativeViewport.projectOpen());
        ws.onRecoverRequested(); row("user presses Recover on that prompt");
        verdict("E02_RECOVER_RELOADED_AND_EMPTIED_UNDO", NativeViewport.fakeUndoDepth() == 0);
    }
    private static void e02ControlAnsweredCandidate() throws Exception {
        freshProcess("E02_control_candidate_answered_at_launch");
        ws.uiState.recordRecoveryResolved();
        NativeViewport.fakeNewProject("C"); ws.noteProjectUnpersisted(); edit("+1"); idle();
        processDeathAndColdLaunch(); row("cold launch: candidate offered");
        ws.onDiscardRecoveryRequested(); row("Discard");
        NativeViewport.fakeNewProject("L"); ws.noteProjectUnpersisted(); edit("+e"); idle(); row("new project, edit, autosave drained");
        recreate(); row("recreate (palette)");
        verdict("CONTROL_NO_PROMPT_WHEN_RESOLVED_WAS_RECORDED", !ws.recoveryPromptVisible);
    }
    private static void e02SavedWorkReoffered() throws Exception {
        freshProcess("E02b_saved_work_offered_as_recovery");
        NativeViewport.fakeNewProject("S"); ws.noteProjectUnpersisted();
        ws.onSaveProjectRequested(); row("Save Project (checkpoint retired)");
        recreate(); row("palette change #1 (recreate)");
        background(); row("app backgrounded (new controller's onStop)");
        recreate(); row("palette change #2 (recreate)");
        verdict("E02b_SAVED_UNCHANGED_PROJECT_OFFERED_AS_UNSAVED_WORK",
                ws.recoveryPromptVisible && "S".equals(new String(ProjectSlot.read(ctx)))
                        && "S".equals(NativeViewport.fakeContent()));
    }

    // ---------------------------------------------------------------- A-06
    private static void a06TranscriptOnly() throws Exception {
        freshProcess("A06_TRANSCRIPT_ONLY_pending_bytes_lost");
        ws.uiState.recordRecoveryResolved();
        NativeViewport.fakeNewProject("P"); ws.noteProjectUnpersisted();
        ws.onSaveCopyRequested(); row("Save Copy... (bytes staged, picker launched)");
        recreate(); row("config change while picker is open (recreate)");
        final String outcome = ws.onCreateProjectDocumentChosen(new Object());
        row("picker returns a destination");
        System.out.println("  outcome=" + outcome + " lastStatus='" + ws.lastStatus + "'");
        verdict("A06_TRANSCRIPT_RESULT_IS_TREATED_AS_CANCEL", outcome.startsWith("CANCELLED"));
    }

    private static void uiStateFields() {
        System.out.println();
        System.out.println("=== EditorUiState declared fields (real class, reflection)");
        for (Field f : EditorUiState.class.getDeclaredFields()) {
            System.out.println("  " + (Modifier.isStatic(f.getModifiers()) ? "static " : "       ")
                    + f.getType().getSimpleName() + " " + f.getName());
        }
    }
}
