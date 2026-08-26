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

        viewport = new ForgeShapeSurfaceView(this);
        viewport.setId(R.id.viewport_surface);
        workspace = new EditorWorkspaceView(this, viewport);
        setContentView(workspace);

        // Without this a text field would take focus at startup, which would
        // both pop the keyboard and swallow the debug key hook below.
        viewport.requestFocus();
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
        if (workspace != null) {
            workspace.syncFromNative();
        }
    }

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

    /** The editor UI, for instrumentation that names a control by its id. */
    EditorWorkspaceView editorWorkspace() {
        return workspace;
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
        if (!isChangingConfigurations()) {
            NativeViewport.stop();
        }
    }
}
