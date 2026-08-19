package com.forgeshape.app;

import android.app.Activity;
import android.os.Bundle;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

/**
 * ForgeShape Android shell.
 *
 * Owns only the Android lifecycle and hosts the ForgeShape viewport surface plus
 * the Construction properties panel that overlays it.
 */
public final class ForgeShapeActivity extends Activity {

    private ForgeShapeSurfaceView viewport;
    private ConstructionPanelView constructionPanel;
    private SculptPanelView sculptPanel;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // Native state — including the Construction box the panel reads its
        // initial values from — must exist before any view asks for it.
        NativeViewport.start();

        final FrameLayout root = new FrameLayout(this);
        viewport = new ForgeShapeSurfaceView(this);
        root.addView(viewport, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        final Runnable modeChanged = new Runnable() {
            @Override
            public void run() {
                syncMode();
            }
        };

        // Both panels are added after the viewport, so they draw on top of the
        // Surface and are offered every touch first. They are anchored at the
        // top deliberately: the soft keyboard comes up from the bottom, so the
        // fields being edited and the object being edited both stay visible.
        //
        // Exactly one of them is ever visible, and which one is decided by
        // NATIVE state, not by the Activity: see syncMode().
        constructionPanel = new ConstructionPanelView(this, viewport, modeChanged);
        root.addView(constructionPanel, panelParams());

        sculptPanel = new SculptPanelView(this, viewport, modeChanged);
        root.addView(sculptPanel, panelParams());

        setContentView(root);
        syncMode();
        // Without this a field would take focus at startup, which would both pop
        // the keyboard and swallow the debug key hook below.
        viewport.requestFocus();
    }

    private FrameLayout.LayoutParams panelParams() {
        final FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.gravity = Gravity.TOP;
        return params;
    }

    /**
     * Shows the panel belonging to the mode native code is actually in, and
     * refreshes it from native truth.
     *
     * <p>The mode is read back rather than assumed, so a refused request (for
     * instance entering Sculpt with nothing frozen) leaves the correct panel on
     * screen instead of one that lies about what is being edited.
     *
     * <p>In Sculpt Mode the shape and transform editors are not merely disabled
     * but absent, so nothing on screen can edit the Construction Source while
     * the Frozen Sculpt Mesh is the thing being worked on.
     */
    private void syncMode() {
        final boolean sculpting = NativeViewport.productMode() == NativeViewport.MODE_SCULPT;
        constructionPanel.setVisibility(sculpting ? View.GONE : View.VISIBLE);
        sculptPanel.setVisibility(sculpting ? View.VISIBLE : View.GONE);
        if (sculpting) {
            sculptPanel.refreshFromNative();
        } else {
            constructionPanel.refreshFromNative();
        }
    }

    /**
     * Re-reads the authoritative native state after a resume.
     *
     * <p>Native state survives home/resume untouched, so this is a display
     * refresh, not a restore: it guarantees the panel on screen is the one for
     * the mode that is actually active, and that its values show what the object
     * actually is — the Construction parameters and placement, or the Frozen
     * Sculpt Mesh and the brush — rather than whatever was left on screen. The
     * selected display unit is preserved for the life of the process.
     */
    @Override
    protected void onResume() {
        super.onResume();
        if (constructionPanel != null && sculptPanel != null) {
            syncMode();
        }
    }

    /**
     * DEBUG-ONLY test hook. The number keys 1-5 publish a native debug mesh
     * fixture and 6-9 drive the native Construction box's authoritative
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

    @Override
    protected void onDestroy() {
        super.onDestroy();
        NativeViewport.stop();
    }
}
