package com.forgeshape.app;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

import android.view.MotionEvent;

import org.junit.Test;

/**
 * The Android half of the pointer tool-type mapping, checked without a device.
 *
 * <p>This is arithmetic on constants — {@code MotionEvent.TOOL_TYPE_*} are
 * compile-time integers, so nothing here touches the Android framework at
 * runtime — which is why it belongs in the JVM suite rather than on a device.
 * The instrumented suite then proves that a real {@link MotionEvent} carries the
 * mapped value all the way into native code.
 *
 * <p>The other half of the contract — ranges, clamping and the non-finite
 * fallbacks for pressure and tilt — is native ForgeShape's and is asserted by
 * the native self-tests, so it is deliberately not restated here.
 */
public final class PointerSemanticsTest {

    @Test
    public void aFingerMapsToTheNeutralFinger() {
        assertEquals(PointerSemantics.TOOL_FINGER,
                PointerSemantics.neutralToolType(MotionEvent.TOOL_TYPE_FINGER));
    }

    @Test
    public void aStylusMapsToTheNeutralStylus() {
        assertEquals(PointerSemantics.TOOL_STYLUS,
                PointerSemantics.neutralToolType(MotionEvent.TOOL_TYPE_STYLUS));
    }

    @Test
    public void anEraserAndAMouseMapToTheirOwnNeutralTypes() {
        assertEquals(PointerSemantics.TOOL_ERASER,
                PointerSemantics.neutralToolType(MotionEvent.TOOL_TYPE_ERASER));
        assertEquals(PointerSemantics.TOOL_MOUSE,
                PointerSemantics.neutralToolType(MotionEvent.TOOL_TYPE_MOUSE));
    }

    @Test
    public void androidsUnknownToolMapsToTheNeutralUnknown() {
        assertEquals(PointerSemantics.TOOL_UNKNOWN,
                PointerSemantics.neutralToolType(MotionEvent.TOOL_TYPE_UNKNOWN));
    }

    /**
     * The fallback that matters: a tool type Android invents after this was
     * written must arrive as Unknown rather than being guessed at, and must not
     * silently become a finger.
     */
    @Test
    public void anUnrecognisedToolTypeFallsBackToUnknown() {
        assertEquals(PointerSemantics.TOOL_UNKNOWN, PointerSemantics.neutralToolType(9999));
        assertEquals(PointerSemantics.TOOL_UNKNOWN, PointerSemantics.neutralToolType(-1));
        assertEquals(PointerSemantics.TOOL_UNKNOWN, PointerSemantics.neutralToolType(
                Integer.MIN_VALUE));
        assertEquals(PointerSemantics.TOOL_UNKNOWN, PointerSemantics.neutralToolType(
                Integer.MAX_VALUE));
    }

    /**
     * The wire codes cross JNI as plain ints and are mirrored by
     * {@code PointerToolType} in {@code forgeshape_input.h}. Two tools sharing a
     * code would make the two sides disagree about what is on the screen.
     */
    @Test
    public void everyWireCodeIsDistinctAndMatchesTheNativeContract() {
        assertEquals(0, PointerSemantics.TOOL_UNKNOWN);
        assertEquals(1, PointerSemantics.TOOL_FINGER);
        assertEquals(2, PointerSemantics.TOOL_STYLUS);
        assertEquals(3, PointerSemantics.TOOL_ERASER);
        assertEquals(4, PointerSemantics.TOOL_MOUSE);
        assertNotEquals(PointerSemantics.TOOL_STYLUS, PointerSemantics.TOOL_ERASER);
    }
}
