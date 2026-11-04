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

    /**
     * A mouse's own navigation (TABLET-WORKBENCH-R1), decided at its button's
     * press and held until its release: the right button orbits, the middle
     * button (or Shift with the right) pans. The left button is not ours — it
     * keeps exactly the meaning a finger has, so a click selects, a drag in a
     * sketch draws and a drag in Sculpt strokes.
     */
    private static final int MOUSE_NAV_NONE = 0;
    private static final int MOUSE_NAV_ORBIT = 1;
    private static final int MOUSE_NAV_PAN = 2;
    private int mouseNav = MOUSE_NAV_NONE;
    private float mouseLastX;
    private float mouseLastY;

    /** Handles a right- or middle-button drag; false for everything else. */
    private boolean handleMouseNavigation(MotionEvent event) {
        final int action = event.getActionMasked();
        if (mouseNav == MOUSE_NAV_NONE) {
            if (action != MotionEvent.ACTION_DOWN
                    || !event.isFromSource(android.view.InputDevice.SOURCE_MOUSE)) {
                return false;
            }
            final int buttons = event.getButtonState();
            if ((buttons & MotionEvent.BUTTON_TERTIARY) != 0
                    || ((buttons & MotionEvent.BUTTON_SECONDARY) != 0
                            && (event.getMetaState() & android.view.KeyEvent.META_SHIFT_ON) != 0)) {
                mouseNav = MOUSE_NAV_PAN;
            } else if ((buttons & MotionEvent.BUTTON_SECONDARY) != 0) {
                mouseNav = MOUSE_NAV_ORBIT;
            } else {
                return false;
            }
            mouseLastX = event.getX();
            mouseLastY = event.getY();
            takeFocusFromEditor();
            return true;
        }
        if (action == MotionEvent.ACTION_MOVE) {
            final float dx = event.getX() - mouseLastX;
            final float dy = event.getY() - mouseLastY;
            mouseLastX = event.getX();
            mouseLastY = event.getY();
            if (mouseNav == MOUSE_NAV_PAN) {
                NativeViewport.cameraPointerPan(dx, dy);
            } else {
                NativeViewport.cameraPointerOrbit(dx, dy);
            }
            return true;
        }
        if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
            mouseNav = MOUSE_NAV_NONE;
            if (gestureSettled != null) {
                gestureSettled.onViewportGestureSettled();
            }
        }
        return true;
    }

    /**
     * The mouse wheel zooms about the pointer (TABLET-WORKBENCH-R1): the point
     * under the cursor stays under it, as a pinch keeps the point between the
     * fingers.
     */
    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        if (event.getActionMasked() == MotionEvent.ACTION_SCROLL
                && event.isFromSource(android.view.InputDevice.SOURCE_CLASS_POINTER)) {
            final float steps = event.getAxisValue(MotionEvent.AXIS_VSCROLL);
            if (steps != 0.0f) {
                NativeViewport.cameraPointerZoom(event.getX(), event.getY(), steps,
                        getWidth(), getHeight());
                if (gestureSettled != null) {
                    gestureSettled.onViewportGestureSettled();
                }
                return true;
            }
        }
        return super.onGenericMotionEvent(event);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (handleMouseNavigation(event)) {
            return true;
        }
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

        // A palm the SYSTEM rejected (`WORKBENCH-UX-R1`): from API 33 the last
        // pointer of a gesture the platform decides was never meant -- a hand
        // resting on the glass beside a pen -- leaves with FLAG_CANCELED. That
        // gesture is then a cancel and never an act: native restores the
        // pre-gesture state and records nothing, the contract a second pointer
        // already has. Real S Pen palm behaviour is a physical-device gate,
        // UNVERIFIED here.
        int forwarded = action;
        if (android.os.Build.VERSION.SDK_INT >= 33 && action == MotionEvent.ACTION_UP
                && (event.getFlags() & MotionEvent.FLAG_CANCELED) != 0) {
            forwarded = MotionEvent.ACTION_CANCEL;
            actionPointerId = -1;
        }

        NativeViewport.touchEvent(forwarded, actionPointerId, count,
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
        } else if (action == MotionEvent.ACTION_MOVE && gestureSettled != null) {
            // World-anchored chrome that must track a LIVE gesture, and only
            // that (`CAD-UX-S1`): the extrude arrow's exact depth changes under
            // the finger, so a value refreshed only when the gesture settles
            // would read the pre-drag number for the whole drag. Nothing about
            // the gesture is interpreted here — the listener re-reads native and
            // decides for itself whether it has anything to move.
            gestureSettled.onViewportGestureMoved();
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

        /**
         * A pointer moved. Deliberately the ONLY per-sample callback, and it
         * exists for exactly one class of listener: world-anchored chrome whose
         * displayed value changes under the finger. It must stay cheap — a full
         * chrome sync here would rewrite panels a user is still typing into.
         */
        void onViewportGestureMoved();

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
