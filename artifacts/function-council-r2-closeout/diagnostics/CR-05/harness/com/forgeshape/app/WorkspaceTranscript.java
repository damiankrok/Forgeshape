package com.forgeshape.app;

import android.content.Context;

/**
 * CR-05: a LINE-FOR-LINE TRANSCRIPTION of the persistence-relevant parts of
 * EditorWorkspaceView.java at 70a6c14 (that class cannot be compiled without
 * the whole Android view stack). Every method cites the product lines it copies;
 * UI-only statements (dismissPrimarySurfaces, showStatus, refreshShellPhase,
 * onNativeStateChanged, setVisibility of chrome) are replaced by a status string
 * and a boolean. scripts in ../transcription_check.sh print the cited product
 * lines so the transcription can be compared by eye. The REAL product classes
 * exercised through this transcript are EditorUiState, AutosaveController,
 * ProjectCheckpoint and ProjectSlot.
 */
final class WorkspaceTranscript {
    // EditorWorkspaceView.java:127
    final EditorUiState uiState = EditorUiState.forNewWorkspace();
    // EditorWorkspaceView.java:271-272  (plain View fields: Java defaults 0 / false)
    long persistedFingerprint;
    boolean everPersisted;
    // :275-279
    static final int LEAVE_NONE = 0, LEAVE_NEW_PROJECT = 1, LEAVE_OPEN_FILE = 2, LEAVE_OPEN_SLOT = 3;
    int pendingLeave = LEAVE_NONE;
    // :285 / :1019
    final AutosaveController autosave;
    // :6004 / :5778
    byte[] pendingCopyBytes;
    byte[] pendingGlbBytes;

    boolean recoveryPromptVisible;
    boolean unsavedPromptVisible;
    String lastStatus = "";
    private final Context context;

    /** Constructor tail, EditorWorkspaceView.java:1019-1030. */
    WorkspaceTranscript(Context context) {
        this.context = context;
        autosave = new AutosaveController(context);          // :1019
        if (!uiState.recoveryResolved()) {                    // :1027
            offerRecoveryIfPresent();                         // :1028
        }
    }

    /** :2597-2602 */
    boolean projectDirty() {
        if (!NativeViewport.projectOpen()) return false;
        return !everPersisted || NativeViewport.projectFingerprint() != persistedFingerprint;
    }
    /** :2605-2608 */
    void noteProjectPersisted() { persistedFingerprint = NativeViewport.projectFingerprint(); everPersisted = true; }
    /** :2611-2614 */
    void noteProjectUnpersisted() { everPersisted = false; persistedFingerprint = 0L; }

    /** :2623-2632 */
    void leaveProjectFor(int intent) {
        if (projectDirty()) { pendingLeave = intent; unsavedPromptVisible = true; return; }
        proceedToLeave(intent);
    }
    /** :2635-2662 (LEAVE_OPEN_FILE's picker launch is out of scope here) */
    void proceedToLeave(int intent) {
        switch (intent) {
            case LEAVE_NEW_PROJECT:
                NativeViewport.closeProject();
                noteProjectUnpersisted();
                uiState.setNewProjectChooserOpen(true);
                break;
            case LEAVE_OPEN_SLOT:
                openSavedProjectSlot();
                break;
            default:
                break;
        }
    }
    /** :2665-2678 */
    void onUnsavedSaveRequested() {
        final int intent = pendingLeave;
        if (!saveProjectToSlot()) { lastStatus = "status_project_save_failed"; return; }
        pendingLeave = LEAVE_NONE;
        unsavedPromptVisible = false;
        proceedToLeave(intent);
    }
    /** :2681-2691 */
    void onUnsavedDiscardRequested() {
        final int intent = pendingLeave;
        pendingLeave = LEAVE_NONE;
        unsavedPromptVisible = false;
        ProjectCheckpoint.clear(context);
        Diagnostics.info(DiagnosticLog.CAT_PERSISTENCE, "UNSAVED_DISCARDED", null);
        proceedToLeave(intent);
    }
    /** :5438-5450 (onSaveProjectRequested) */
    void onSaveProjectRequested() {
        if (!saveProjectToSlot()) { lastStatus = "status_project_save_failed"; return; }
        lastStatus = "status_project_saved";
    }
    /** :5458-5472 */
    boolean saveProjectToSlot() {
        final byte[] bytes = NativeViewport.encodeProject();
        if (bytes == null || bytes.length == 0 || !ProjectSlot.write(context, bytes)) return false;
        ProjectCheckpoint.clear(context);
        autosave.noteProjectPersisted();
        noteProjectPersisted();
        Diagnostics.info(DiagnosticLog.CAT_PERSISTENCE, "MANUAL_SAVE", "bytes=" + bytes.length);
        return true;
    }
    /** :5478-5481 */
    void onOpenProjectRequested() { leaveProjectFor(LEAVE_OPEN_SLOT); }
    /** :5483-5513 */
    void openSavedProjectSlot() {
        final byte[] bytes = ProjectSlot.read(context);
        if (bytes == null) { lastStatus = "status_project_none"; return; }
        final int status = NativeViewport.loadProject(bytes);
        if (status != NativeViewport.PROJECT_OK) { lastStatus = "load_failed"; return; }
        ProjectCheckpoint.clear(context);
        autosave.noteProjectPersisted();
        noteProjectPersisted();
        Diagnostics.info(DiagnosticLog.CAT_PERSISTENCE, "MANUAL_OPEN", "bytes=" + bytes.length);
        lastStatus = "status_project_opened";
    }
    /** :5540-5552 */
    void noteProjectMaybeDirty() { autosave.noteMaybeDirty(); }
    void requestImmediateCheckpoint() { autosave.requestImmediateCheckpoint(); }
    /** :5555-5557 */
    void releaseAutosave() { autosave.release(); }

    /** :5579-5604 */
    boolean offerRecoveryIfPresent() {
        if (!ProjectCheckpoint.exists(context)) return false;
        final byte[] bytes = ProjectCheckpoint.read(context);
        final int status = bytes == null ? NativeViewport.PROJECT_NO_DATA : NativeViewport.validateProject(bytes);
        if (status != NativeViewport.PROJECT_OK) {
            ProjectCheckpoint.quarantine(context);
            uiState.recordRecoveryResolved();
            lastStatus = "status_recovery_failed";
            return false;
        }
        Diagnostics.info(DiagnosticLog.CAT_RECOVERY, "CANDIDATE_OFFERED", "bytes=" + bytes.length);
        recoveryPromptVisible = true;
        return true;
    }
    /** :5652-5687 */
    void onRecoverRequested() {
        uiState.recordRecoveryResolved();
        recoveryPromptVisible = false;
        final byte[] bytes = ProjectCheckpoint.read(context);
        final int status = bytes == null ? NativeViewport.PROJECT_NO_DATA : NativeViewport.loadProject(bytes);
        if (status != NativeViewport.PROJECT_OK) {
            ProjectCheckpoint.quarantine(context);
            lastStatus = "status_recovery_failed";
            return;
        }
        ProjectCheckpoint.clear(context);   // :5675
        autosave.noteProjectPersisted();    // :5676
        noteProjectPersisted();             // :5679
        Diagnostics.info(DiagnosticLog.CAT_RECOVERY, "RECOVERED", null);
        lastStatus = "status_recovery_recovered";
    }
    /** :5689-5701 */
    void onDiscardRecoveryRequested() {
        uiState.recordRecoveryResolved();
        recoveryPromptVisible = false;
        ProjectCheckpoint.clear(context);
        Diagnostics.info(DiagnosticLog.CAT_RECOVERY, "DISCARDED", null);
    }
    /** onOpenProjectDocumentChosen success tail, :6104-6111 (method starts :6077) (for the Open File control) */
    void openFileApplied(byte[] bytes) {
        if (NativeViewport.loadProject(bytes) != NativeViewport.PROJECT_OK) return;
        noteProjectPersisted();
        noteProjectMaybeDirty();
        lastStatus = "status_project_file_opened";
    }
    /** :5709-5726 (picker launch replaced by "launched") */
    void onSaveCopyRequested() {
        final byte[] bytes = NativeViewport.encodeProject();
        if (bytes == null || bytes.length == 0) { lastStatus = "status_project_no_encode"; return; }
        pendingCopyBytes = bytes;
    }
    /** :6047-6064 ; the ContentResolver write is replaced by a recorded flag */
    String onCreateProjectDocumentChosen(Object destination) {
        final byte[] bytes = pendingCopyBytes;
        pendingCopyBytes = null;
        if (destination == null || bytes == null) {
            Diagnostics.info(DiagnosticLog.CAT_TRANSFER, "SAVE_COPY_CANCELLED", null);
            return "CANCELLED(no status shown)";
        }
        lastStatus = "status_project_copy_saved";
        return "WROTE " + bytes.length + " bytes";
    }
}
