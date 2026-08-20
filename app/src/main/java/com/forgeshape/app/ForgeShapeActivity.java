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
        super.onCreate(savedInstanceState);
        // Native state — including the Construction object the inspector reads
        // its initial values from — must exist before any view asks for it.
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
     * {@code adb shell input keyevent} without any product UI. Native code makes
     * this a no-op in a release build, in which case the key is not consumed.
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

    @Override
    protected void onDestroy() {
        super.onDestroy();
        NativeViewport.stop();
    }
}
