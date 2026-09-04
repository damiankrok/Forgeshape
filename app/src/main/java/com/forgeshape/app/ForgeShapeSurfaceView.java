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

    // Reused across events. Touch delivery is single-threaded on the UI thread,
    // and a MOVE arrives at the display rate, so nothing here may allocate.
    private final int[] pointerIds = new int[MAX_POINTERS];
    private final float[] pointerXs = new float[MAX_POINTERS];
    private final float[] pointerYs = new float[MAX_POINTERS];

    // Stylus semantics, carried across the boundary for Sketch and Sculpt to
    // use later. Nothing in this view interprets them, and nothing downstream
    // consumes them yet: a stylus and a finger tracing the same pixels still
    // produce exactly the same result. Ranges and fallbacks are native
    // ForgeShape's to own -- see forgeshape_input.h -- so these arrays carry
    // what Android reported, unrepaired, apart from the tool-type mapping.
    private final int[] pointerToolTypes = new int[MAX_POINTERS];
    private final float[] pointerPressures = new float[MAX_POINTERS];
    private final float[] pointerTilts = new float[MAX_POINTERS];
    private final float[] pointerTiltOrientations = new float[MAX_POINTERS];

    private OnViewportGestureSettled gestureSettled;

    /** Whether a pointer is currently down on the viewport. UI-thread only,
     *  presentation only: the gesture itself is still native code's to read. */
    private boolean pointerDown;

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
            // A viewport gesture has begun. Nothing about it is interpreted
            // here — the listener is told only that the user's pointer is on
            // the model, which is what chrome uses to decide it must not spend
            // time animating. See PropertyInspectorView#setMotionAllowed.
            pointerDown = true;
            if (gestureSettled != null) {
                gestureSettled.onViewportGestureStarted();
            }
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
            pointerToolTypes[i] = PointerSemantics.neutralToolType(event.getToolType(i));
            pointerPressures[i] = event.getPressure(i);
            // AXIS_TILT is radians from perpendicular and reads 0 on hardware
            // that cannot measure it; getOrientation is radians in the screen
            // plane. Both are passed through as reported -- native code decides
            // what a missing or nonsensical value means.
            pointerTilts[i] = event.getAxisValue(MotionEvent.AXIS_TILT, i);
            pointerTiltOrientations[i] = event.getOrientation(i);
        }

        // Only up-style actions designate a specific pointer that is leaving.
        int actionPointerId = -1;
        if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP) {
            actionPointerId = event.getPointerId(event.getActionIndex());
        }

        NativeViewport.touchEvent(action, actionPointerId, count,
                pointerIds, pointerXs, pointerYs,
                pointerToolTypes, pointerPressures, pointerTilts, pointerTiltOrientations,
                getWidth(), getHeight());

        // A gesture that ended may have resolved a tap, and a tap that hit a
        // body makes that body the edit target down in native code. Nothing is
        // interpreted here — the listener only learns that a gesture settled,
        // and decides for itself whether anything it displays actually moved.
        if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
            pointerDown = false;
            if (gestureSettled != null) {
                gestureSettled.onViewportGestureSettled();
            }
        }
        return true;
    }

    /** Whether the user currently has a pointer down on the model. */
    boolean viewportPointerDown() {
        return pointerDown;
    }

    /**
     * Stylus hover over the viewport (`CAD-A3` K1).
     *
     * <p>A hovering stylus is not a touch: nothing is pressed, so nothing may
     * be selected, drawn, orbited or committed by it. The one thing it may do
     * is HIGHLIGHT, and today the one surface that highlights is the spatial
     * support chooser, whose target under the pen lights up so the tap that
     * follows is aimed. Native code decides whether a chooser is active at
     * all; when none is, the event is simply not ours and the platform's own
     * hover handling continues. A finger never hovers, so this changes nothing
     * for touch.
     */
    @Override
    public boolean onHoverEvent(MotionEvent event) {
        final int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_HOVER_MOVE
                || action == MotionEvent.ACTION_HOVER_ENTER) {
            if (NativeViewport.supportChooserHover(event.getX(), event.getY()) >= 0) {
                lastHoverHighlighted = true;
                return true;
            }
            lastHoverHighlighted = false;
        }
        return super.onHoverEvent(event);
    }

    /** Whether the last hover sample lit a chooser target, for verification. */
    boolean lastHoverHighlighted() {
        return lastHoverHighlighted;
    }

    private boolean lastHoverHighlighted;

    /** Told when a viewport gesture starts, and when it has finished and native
     *  state may have moved. */
    interface OnViewportGestureSettled {
        void onViewportGestureStarted();

        void onViewportGestureSettled();
    }

    void setOnViewportGestureSettled(OnViewportGestureSettled listener) {
        gestureSettled = listener;
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
