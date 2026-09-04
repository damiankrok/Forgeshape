package com.forgeshape.app;

import android.app.Activity;
import android.os.Build;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.View;

/**
 * ForgeShape Android shell.
 *
 * <p>Owns the Android lifecycle, the window's edge-to-edge configuration, and
 * nothing else. The editor UI is {@link EditorWorkspaceView}; the geometry,
 * the camera, the modes and the render loop are native.
 */
public final class ForgeShapeActivity extends Activity {

    private ForgeShapeSurfaceView viewport;
    private EditorWorkspaceView workspace;

    /** API 33+ only; null below, where {@link #onBackPressed()} is the route. */
    private BackDismissal backDismissal;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // BEFORE super.onCreate and before anything is inflated: a theme applied
        // later would leave every already-resolved drawable and colour state
        // list holding the previous theme's answers.
        applyTheme();

        super.onCreate(savedInstanceState);
        // Native state — including the Construction object the inspector reads
        // its initial values from — must exist before any view asks for it.
        // Started once per PROCESS: a second call while the render thread is
        // alive returns immediately, which is what makes a theme recreation
        // cost nothing (see onDestroy).
        NativeViewport.start();

        goEdgeToEdge();

        // Installed before the workspace is built, so a failure while building
        // it is still reported. Chains to whatever handler was already there:
        // Android's own crash reporting and the process kill both still happen.
        Diagnostics.installUncaughtHandler(this);
        Diagnostics.notePreviousExit(this);
        Diagnostics.info(DiagnosticLog.CAT_LIFECYCLE, "ACTIVITY_CREATE",
                savedInstanceState == null ? "fresh" : "restored");

        viewport = new ForgeShapeSurfaceView(this);
        viewport.setId(R.id.viewport_surface);
        workspace = new EditorWorkspaceView(this, viewport);
        workspace.setProjectTransferHost(transferHost);
        setContentView(workspace);

        // System Back closes an open context surface before it leaves the app.
        // See installBackDismissal.
        installBackDismissal();

        // Without this a text field would take focus at startup, which would
        // both pop the keyboard and swallow the debug key hook below.
        viewport.requestFocus();
    }

    /**
     * Makes System Back dismiss an open context surface before it leaves the
     * app.
     *
     * <p>With the Add Primitive palette open, one Back press used to return the
     * launcher. The platform rule is not negotiable — Back always works and is
     * never trapped — but "works" means the innermost thing the user opened
     * closes first. On a modeling tool the difference between "closed a palette"
     * and "left the app" is the whole of what an accidental Back costs.
     *
     * <p><b>Registered only while there is something to dismiss</b>, which is
     * what keeps the platform's own exit behaviour — including the predictive
     * back animation — untouched for the press that really does leave. The
     * workspace reports the transition; nothing here polls.
     *
     * <p>Two paths, because the platform has two. An app targeting SDK 36 gets
     * predictive back by default, and the system stops calling
     * {@code onBackPressed} entirely, so the dispatcher is the only route on
     * API 33 and above. Below that the override is the only route. The DECISION
     * — what a Back press means — is one method either way; see
     * {@link EditorWorkspaceView#dismissTopmostSurface()}.
     */
    private void installBackDismissal() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            backDismissal = new BackDismissal(this);
        }
        workspace.setOnDismissibleSurfaceChanged(new
                EditorWorkspaceView.OnDismissibleSurfaceChanged() {
            @Override
            public void onDismissibleSurfaceChanged(boolean present) {
                if (backDismissal != null) {
                    backDismissal.setPresent(present);
                }
            }
        });
    }

    /**
     * The pre-API-33 route. Deprecated by the platform, and still the only one
     * an older release calls.
     */
    @Override
    @SuppressWarnings("deprecation")
    public void onBackPressed() {
        if (workspace != null && workspace.dismissTopmostSurface()) {
            return;
        }
        super.onBackPressed();
    }

    /**
     * The API 33+ route: one callback, registered exactly while the workspace
     * has a surface to dismiss.
     *
     * <p>A nested class so nothing on an older release ever has to resolve
     * {@code OnBackInvokedCallback}, which does not exist there.
     */
    private static final class BackDismissal {

        private final ForgeShapeActivity activity;
        private final android.window.OnBackInvokedCallback callback;
        private boolean registered;

        BackDismissal(final ForgeShapeActivity activity) {
            this.activity = activity;
            this.callback = new android.window.OnBackInvokedCallback() {
                @Override
                public void onBackInvoked() {
                    if (activity.workspace == null
                            || !activity.workspace.dismissTopmostSurface()) {
                        // Registered only while there was something to dismiss,
                        // so this is the state having changed under the press.
                        // The default is still what Back means.
                        activity.finish();
                    }
                }
            };
        }

        void setPresent(boolean present) {
            if (present == registered) {
                return;
            }
            registered = present;
            if (present) {
                activity.getOnBackInvokedDispatcher().registerOnBackInvokedCallback(
                        android.window.OnBackInvokedDispatcher.PRIORITY_DEFAULT, callback);
            } else {
                activity.getOnBackInvokedDispatcher()
                        .unregisterOnBackInvokedCallback(callback);
            }
        }

        /** Whether the callback is on the dispatcher right now. Verification. */
        boolean registered() {
            return registered;
        }
    }

    /** For verification: whether Back is currently ours to consume. */
    boolean backDismissalArmed() {
        return backDismissal != null && backDismissal.registered();
    }

    /**
     * Puts this Activity into the appearance the process has chosen, and tells
     * native code what the viewport should be cleared to.
     *
     * <p>Both halves come from the <b>one</b> UI-owned choice in
     * {@link EditorUiState}, so the chrome and the viewport cannot disagree
     * about which theme is in force. The derivation runs here and nowhere else.
     *
     * <p>The native call is the whole of what a theme means below JNI: a closed
     * viewport appearance index. No Android theme, no style, no colour authored
     * in Java and no Android type crosses. It publishes no mesh, mints no
     * revision, rebuilds no geometry and re-uploads nothing.
     */
    private void applyTheme() {
        final AppTheme theme = EditorUiState.currentAppTheme();
        setTheme(theme.styleRes());
        NativeViewport.setViewportBackground(theme.viewportBackground());
    }

    /**
     * Switches the appearance, by recreating this Activity.
     *
     * <p>Recreation rather than a manual repaint, and that is a deliberate
     * choice rather than a shortcut. The workspace is built entirely in code
     * from themed resources, so re-resolving them means rebuilding the views
     * that hold them — and walking dozens of view classes reapplying colours
     * would be the duplication the theme attributes exist to avoid, with every
     * surface a chance to be missed.
     *
     * <p>It is safe because <b>nothing that matters lives in the Activity</b>.
     * The scene, every body, the active {@code ObjectId}, the product mode, the
     * Frozen Sculpt Mesh, the camera and the display settings are all
     * process-scoped native state that outlives this object; the start choice
     * and the theme are process-scoped UI state for the same reason. What is
     * destroyed and rebuilt is the view tree, which owns none of it.
     *
     * <p>It is also <b>free</b>, because {@link #onDestroy} does not stop native
     * code during a configuration change: the render thread, the Vulkan device
     * and every GPU buffer survive, so the Surface is detached and reattached
     * exactly as it is on a HOME/resume — with no re-upload and no self-test
     * re-run.
     */
    void requestTheme(AppTheme theme) {
        if (!EditorUiState.setCurrentAppTheme(theme)) {
            return;  // already wearing it; recreating would flash for nothing
        }
        // The session state goes with it. Changing colour must not also snap
        // the display unit back to meters, close the Property Inspector or
        // re-point the Tool Rail — none of which the user asked for.
        if (workspace != null) {
            EditorUiState.carryAcrossRecreation(workspace.uiState());
        }
        recreate();
    }

    /**
     * Draws the window behind the system bars.
     *
     * <p>The app used to get a bare viewport from the deprecated fullscreen
     * theme, which simply hid the bars. Under targetSdk 36 that is no longer
     * guaranteed, so the window now draws behind transparent bars and
     * {@link EditorWorkspaceView} pads its chrome off them and off any display
     * cutout. The Vulkan surface still gets the whole window, which is the
     * point: the layout never resizes it.
     */
    private void goEdgeToEdge() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            getWindow().setDecorFitsSystemWindows(false);
            return;
        }
        // minSdk is 26; on those releases the flag-based API is the only one.
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
    }

    /**
     * Re-reads the authoritative native state after a resume.
     *
     * <p>Native state survives home/resume untouched, so this is a display
     * refresh, not a restore: it guarantees the surfaces on screen are the ones
     * for the mode that is actually active, and that their values show what the
     * object actually is. The selected display unit and the panel layout state
     * are UI-owned and are preserved for the life of the process.
     */
    @Override
    protected void onResume() {
        super.onResume();
        Diagnostics.info(DiagnosticLog.CAT_LIFECYCLE, "ACTIVITY_RESUME", null);
        if (workspace != null) {
            workspace.syncFromNative();
            // Coming back is also the moment to notice that the renderer did
            // not. See checkRendererLifecycle.
            checkRendererLifecycle();
        }
    }

    /**
     * Checkpoints the project on the way out of the foreground.
     *
     * <p>{@code onStop} rather than {@code onPause}: pause fires for a dialog
     * over the app, and checkpointing for one would be a write on every
     * incidental interruption. Stop is the edge that actually means "this
     * process may not be here when you look again", and it is the last
     * guaranteed callback before Android may kill it.
     *
     * <p>The request is immediate rather than debounced, because a debounce
     * assumes a later moment that may not come.
     */
    @Override
    protected void onStop() {
        super.onStop();
        Diagnostics.info(DiagnosticLog.CAT_LIFECYCLE, "ACTIVITY_STOP",
                isChangingConfigurations() ? "config" : "background");
        if (workspace != null) {
            workspace.requestImmediateCheckpoint();
        }
    }

    /**
     * Notices that the viewport has stopped for good, and tells the workspace.
     *
     * <p>Polled at the two moments it can matter — a resume, and a debug
     * injection's follow-up — rather than watched continuously. A renderer that
     * has died stays dead, so there is nothing to miss by asking twice instead
     * of subscribing, and a callback from the render thread into Java would be a
     * new cross-thread path for a state that changes at most once per process.
     */
    void checkRendererLifecycle() {
        if (workspace == null || rendererRestartReported) {
            return;
        }
        if (NativeViewport.rendererLifecycle() != NativeViewport.RENDERER_RESTART_REQUIRED) {
            return;
        }
        // Reported once. The message stands until the user acts on it, and
        // repeating it on every resume would bury whatever they did next.
        rendererRestartReported = true;
        workspace.onRendererRestartRequired();
    }

    private boolean rendererRestartReported;

    /**
     * DEBUG-ONLY test hook. The number keys 1-5 publish a native debug mesh
     * fixture and 6-9 drive the native Construction object's authoritative
     * dimensions, so both paths can be exercised from
     * {@code adb shell input keyevent} without any product UI. A-E publish the
     * Gate P1 heavy-mesh density ladder (~10k/50k/100k/250k/500k vertices) and F
     * freezes whichever tier was published most recently into Sculpt, for
     * render/upload/picking/Sculpt measurement at densities no real primitive
     * produces. Native code makes this a no-op in a release build, in which
     * case the key is not consumed.
     *
     * <p>This is not a product feature and nothing user-facing exposes it. In
     * particular, keys 6-9 are not dimension editing: they are a bounded test
     * driver for parameter updates, and no UI, persistence or undo exists.
     */
    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        final int command;
        switch (keyCode) {
            case KeyEvent.KEYCODE_1: command = 1; break;  // Fixture A: baseline
            case KeyEvent.KEYCODE_2: command = 2; break;  // Fixture B: same topology
            case KeyEvent.KEYCODE_3: command = 3; break;  // Fixture C: larger
            case KeyEvent.KEYCODE_4: command = 4; break;  // repeated-update stress
            case KeyEvent.KEYCODE_5: command = 5; break;  // dump diagnostics
            case KeyEvent.KEYCODE_6: command = 6; break;  // box state A: 2.0 x 1.0 x 0.5 m
            case KeyEvent.KEYCODE_7: command = 7; break;  // box state B: 1.25 x 2.5 x 0.75 m
            case KeyEvent.KEYCODE_8: command = 8; break;  // box state C: 3.333 x 0.42 x 1.125 m
            case KeyEvent.KEYCODE_9: command = 9; break;  // invalid dimension: must be rejected
            case KeyEvent.KEYCODE_A: command = 17; break;  // stress mesh tier: ~10k vertices
            case KeyEvent.KEYCODE_B: command = 18; break;  // stress mesh tier: ~50k vertices
            case KeyEvent.KEYCODE_C: command = 19; break;  // stress mesh tier: ~100k vertices
            case KeyEvent.KEYCODE_D: command = 20; break;  // stress mesh tier: ~250k vertices
            case KeyEvent.KEYCODE_E: command = 21; break;  // stress mesh tier: ~500k vertices
            case KeyEvent.KEYCODE_F: command = 22; break;  // freeze last stress tier to Sculpt
            // Where the gizmo's pivot and handles are on screen right now, so a
            // walkthrough can drive a real drag against a real handle without
            // ever writing a coordinate down. Reports only; grabs nothing.
            case KeyEvent.KEYCODE_G: command = 23; break;  // log gizmo handle pixels
            default: return super.onKeyDown(keyCode, event);
        }
        if (NativeViewport.debugMeshCommand(command)) {
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }

    // -----------------------------------------------------------------------
    // Project transfer through the Storage Access Framework
    // -----------------------------------------------------------------------
    //
    // The Activity owns this half because only an Activity can start a picker
    // and receive its answer. It owns nothing else about it: it does not know
    // what a project is, does not read or write a byte, and hands the workspace
    // a {@code Uri} and nothing more. The Uri itself stops at
    // {@link ProjectTransfer}; no layer below that ever sees one, and none of it
    // is ever stored as project truth.
    //
    // startActivityForResult rather than the AndroidX Activity Result APIs
    // because this module has no AndroidX runtime dependency and is not about to
    // acquire one for five intents.

    private static final int REQUEST_CREATE_PROJECT_DOCUMENT = 4101;
    private static final int REQUEST_OPEN_PROJECT_DOCUMENT = 4102;
    private static final int REQUEST_CREATE_DIAGNOSTICS_DOCUMENT = 4103;
    private static final int REQUEST_CREATE_GLB_DOCUMENT = 4104;
    private static final int REQUEST_OPEN_GLB_DOCUMENT = 4105;

    private final EditorWorkspaceView.ProjectTransferHost transferHost =
            new EditorWorkspaceView.ProjectTransferHost() {
                @Override
                public boolean requestCreateProjectDocument() {
                    return launch(ProjectTransfer.createDocumentIntent(),
                            REQUEST_CREATE_PROJECT_DOCUMENT);
                }

                @Override
                public boolean requestOpenProjectDocument() {
                    return launch(ProjectTransfer.openDocumentIntent(),
                            REQUEST_OPEN_PROJECT_DOCUMENT);
                }

                @Override
                public boolean requestCreateDiagnosticsDocument() {
                    return launch(ProjectTransfer.createDiagnosticsIntent(),
                            REQUEST_CREATE_DIAGNOSTICS_DOCUMENT);
                }

                @Override
                public boolean requestCreateGlbDocument() {
                    return launch(ProjectTransfer.createGlbDocumentIntent(),
                            REQUEST_CREATE_GLB_DOCUMENT);
                }

                @Override
                public boolean requestOpenGlbDocument() {
                    return launch(ProjectTransfer.openGlbDocumentIntent(),
                            REQUEST_OPEN_GLB_DOCUMENT);
                }
            };

    /**
     * Starts a system picker, or reports that there is none.
     *
     * <p>A device with no documents UI is unusual but not impossible, and an
     * {@code ActivityNotFoundException} escaping here would crash the app for
     * pressing a menu item. Returning false lets the workspace say so instead.
     */
    private boolean launch(android.content.Intent intent, int requestCode) {
        try {
            startActivityForResult(intent, requestCode);
            return true;
        } catch (android.content.ActivityNotFoundException error) {
            Diagnostics.warn(DiagnosticLog.CAT_TRANSFER, "NO_DOCUMENT_PICKER",
                    String.valueOf(requestCode));
            return false;
        }
    }

    /**
     * The picker's answer.
     *
     * <p>A cancel — {@code RESULT_CANCELED}, or a result with no data — is
     * routed through the same handlers with a null {@code Uri}, so "the user
     * backed out" is one honest no-op rather than a silently different path.
     */
    @Override
    @SuppressWarnings("deprecation")
    protected void onActivityResult(int requestCode, int resultCode, android.content.Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (workspace == null) {
            return;
        }
        final android.net.Uri uri =
                (resultCode == RESULT_OK && data != null) ? data.getData() : null;
        switch (requestCode) {
            case REQUEST_CREATE_PROJECT_DOCUMENT:
                workspace.onCreateProjectDocumentChosen(uri);
                break;
            case REQUEST_OPEN_PROJECT_DOCUMENT:
                workspace.onOpenProjectDocumentChosen(uri);
                break;
            case REQUEST_CREATE_DIAGNOSTICS_DOCUMENT:
                workspace.onCreateDiagnosticsDocumentChosen(uri);
                break;
            case REQUEST_CREATE_GLB_DOCUMENT:
                workspace.onCreateGlbDocumentChosen(uri);
                break;
            case REQUEST_OPEN_GLB_DOCUMENT:
                workspace.onOpenGlbDocumentChosen(uri);
                break;
            default:
                break;
        }
    }

    /** The editor UI, for instrumentation that names a control by its id. */
    EditorWorkspaceView editorWorkspace() {
        return workspace;
    }

    /**
     * The real picker host, so a case that swaps in a recording one — a system
     * document picker cannot be driven from instrumentation — can put it back.
     */
    EditorWorkspaceView.ProjectTransferHost projectTransferHost() {
        return transferHost;
    }

    /**
     * Stops native code only when this Activity is really going away.
     *
     * <p>{@code stop()} joins the render thread, which destroys the Vulkan
     * instance, the device and every GPU buffer with it — so the next
     * {@code start()} would re-run the self-tests, re-publish and re-upload the
     * whole scene. That is correct when the app is finishing and quite wrong for
     * a theme change, which must cost the model nothing.
     *
     * <p>During a configuration change — a theme recreation, or a window change
     * `configChanges` did not absorb — the process, the scene and the render
     * thread all continue. The Surface is detached and reattached, exactly as on
     * a HOME/resume, and the buffers are device-scoped so nothing is re-uploaded.
     */
    @Override
    protected void onDestroy() {
        super.onDestroy();
        // ALWAYS, configuration change included, and that is the difference
        // between this and native code. The render thread and every GPU buffer
        // are expensive and are deliberately kept alive across a recreation; the
        // autosave worker is one thread that the rebuilt workspace immediately
        // replaces with its own, so keeping the old one would leak a thread per
        // theme change. Releasing does not cancel the checkpoint `onStop` just
        // asked for — see AutosaveController#release.
        if (workspace != null) {
            workspace.releaseAutosave();
        }
        if (!isChangingConfigurations()) {
            NativeViewport.stop();
        }
    }
}
