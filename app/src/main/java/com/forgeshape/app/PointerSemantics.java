package com.forgeshape.app;

import android.view.MotionEvent;

/**
 * The Android side of the platform-neutral pointer contract.
 *
 * <p>This is the ONLY place where an Android tool-type constant is turned into
 * something ForgeShape's domain understands. Native code never sees
 * {@code MotionEvent.TOOL_TYPE_*}: it sees the small wire codes below, which are
 * ForgeShape's own and which {@code forgeshape_input.h} mirrors as
 * {@code PointerToolType}. That is what keeps the domain platform-neutral while
 * this class stays an adapter and nothing more.
 *
 * <p>Deliberately no state, no listener and no interpretation. Deciding what a
 * gesture <i>means</i> is native ForgeShape's job, and pressure and tilt are
 * carried across the boundary raw — native code owns their range and fallback
 * rules so the two sides cannot disagree about them.
 */
final class PointerSemantics {

    private PointerSemantics() {
    }

    // The wire codes. These must stay identical to PointerToolType in
    // app/src/main/cpp/forgeshape_input.h; they cross JNI as plain ints.

    /** Something the platform reports that ForgeShape does not model. Still an
     *  ordinary contact pointer — never a reason to drop the event. */
    static final int TOOL_UNKNOWN = 0;

    /** A finger. The default for every existing touch flow. */
    static final int TOOL_FINGER = 1;

    /** A stylus tip. */
    static final int TOOL_STYLUS = 2;

    /** A stylus held reversed. Carried only: it switches no brush and no tool. */
    static final int TOOL_ERASER = 3;

    /** A mouse or trackpad. Carried only: no wheel, hover or context behaviour. */
    static final int TOOL_MOUSE = 4;

    /**
     * Maps one Android tool type onto its neutral wire code.
     *
     * <p>Anything outside the set ForgeShape models — including a tool type a
     * future Android release invents — becomes {@link #TOOL_UNKNOWN} rather
     * than being guessed at or rejected.
     *
     * @param androidToolType a {@code MotionEvent.TOOL_TYPE_*} value
     */
    static int neutralToolType(int androidToolType) {
        switch (androidToolType) {
            case MotionEvent.TOOL_TYPE_FINGER:
                return TOOL_FINGER;
            case MotionEvent.TOOL_TYPE_STYLUS:
                return TOOL_STYLUS;
            case MotionEvent.TOOL_TYPE_ERASER:
                return TOOL_ERASER;
            case MotionEvent.TOOL_TYPE_MOUSE:
                return TOOL_MOUSE;
            default:
                // TOOL_TYPE_UNKNOWN, and anything added after this was written.
                return TOOL_UNKNOWN;
        }
    }
}
