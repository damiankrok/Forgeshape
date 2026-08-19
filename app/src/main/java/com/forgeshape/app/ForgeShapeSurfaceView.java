package com.forgeshape.app;

import android.content.Context;
import android.util.Log;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.inputmethod.InputMethodManager;

/**
 * ForgeShape-owned viewport surface.
 *
 * <p>The view owns nothing but the Android Surface lifecycle and raw pointer
 * forwarding; all rendering and camera truth lives in the native ForgeShape
 * code. In particular this class holds no yaw/pitch/distance, builds no
 * matrices, and does no world-space math. No {@code GestureDetector} or
 * {@code ScaleGestureDetector} is used, deliberately: interpreting the gesture
 * is native ForgeShape's job.
 */
final class ForgeShapeSurfaceView extends SurfaceView implements SurfaceHolder.Callback {

    private static final String TAG = "ForgeShape";

    /** Pointers beyond this are ignored; navigation only ever needs two. */
    private static final int MAX_POINTERS = 6;

    // Reused across events. Touch delivery is single-threaded on the UI thread.
    private final int[] pointerIds = new int[MAX_POINTERS];
    private final float[] pointerXs = new float[MAX_POINTERS];
    private final float[] pointerYs = new float[MAX_POINTERS];

    ForgeShapeSurfaceView(Context context) {
        super(context);
        getHolder().addCallback(this);
        setFocusable(true);
        setFocusableInTouchMode(true);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        final int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN) {
            // Touching the viewport ends any text edit in the overlay panel:
            // focus and the soft keyboard both come back here, so navigating the
            // model never happens "through" a focused field. Nothing about the
            // gesture itself is interpreted by this — that is still native work.
            takeFocusFromEditor();
        }
        int count = event.getPointerCount();
        if (count > MAX_POINTERS) {
            count = MAX_POINTERS;
        }
        for (int i = 0; i < count; i++) {
            pointerIds[i] = event.getPointerId(i);
            pointerXs[i] = event.getX(i);
            pointerYs[i] = event.getY(i);
        }

        // Only up-style actions designate a specific pointer that is leaving.
        int actionPointerId = -1;
        if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP) {
            actionPointerId = event.getPointerId(event.getActionIndex());
        }

        NativeViewport.touchEvent(action, actionPointerId, count,
                pointerIds, pointerXs, pointerYs, getWidth(), getHeight());
        return true;
    }

    /** Pulls focus and the soft keyboard away from whatever was being edited. */
    private void takeFocusFromEditor() {
        if (isFocused()) {
            return;
        }
        requestFocus();
        final InputMethodManager ime =
                (InputMethodManager) getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        if (ime != null) {
            ime.hideSoftInputFromWindow(getWindowToken(), 0);
        }
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        Log.i(TAG, "SurfaceView: surfaceCreated");
        NativeViewport.surfaceCreated(holder.getSurface());
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        Log.i(TAG, "SurfaceView: surfaceChanged " + width + "x" + height);
        NativeViewport.surfaceChanged(width, height);
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.i(TAG, "SurfaceView: surfaceDestroyed");
        // Must block until native code has released the ANativeWindow.
        NativeViewport.surfaceDestroyed();
    }
}
