package com.forgeshape.app;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

/**
 * The display-settings index contract, from the Java side.
 *
 * <p>The shading model and the surface shading cross JNI as plain ints, and
 * native code maps those ints onto closed enums by their numeric order. That
 * correspondence is the contract: if the Java constants and the native
 * enumerators ever disagree, every display control silently selects the wrong
 * mode and nothing fails loudly. The native shading suite pins its side; this
 * pins ours.
 *
 * <p>This runs on the JVM with no device and no native library. That is safe
 * because {@code static final int} initialisers are compile-time constants and
 * are inlined by the compiler, so naming one here never triggers
 * {@code NativeViewport}'s class initialisation or its
 * {@code System.loadLibrary}.
 */
public final class DisplaySettingsContractTest {

    @Test
    public void shadingModelIndicesAreZeroBasedAndContiguous() {
        assertEquals("Studio Solid is index 0 and therefore the native default",
                0, NativeViewport.SHADING_STUDIO);
        assertEquals("MatCap is index 1", 1, NativeViewport.SHADING_MATCAP);
        assertEquals("the debug source-colour path is index 2",
                2, NativeViewport.SHADING_DEBUG_SOURCE_COLOR);
    }

    @Test
    public void surfaceShadingIndicesAreZeroBasedAndContiguous() {
        assertEquals("Smooth is index 0 and therefore the native default",
                0, NativeViewport.SURFACE_SMOOTH);
        assertEquals("Faceted is index 1", 1, NativeViewport.SURFACE_FACETED);
    }

    @Test
    public void everyShadingModelIsDistinct() {
        assertNotEquals(NativeViewport.SHADING_STUDIO, NativeViewport.SHADING_MATCAP);
        assertNotEquals(NativeViewport.SHADING_STUDIO, NativeViewport.SHADING_DEBUG_SOURCE_COLOR);
        assertNotEquals(NativeViewport.SHADING_MATCAP, NativeViewport.SHADING_DEBUG_SOURCE_COLOR);
        assertNotEquals(NativeViewport.SURFACE_SMOOTH, NativeViewport.SURFACE_FACETED);
    }

    /**
     * The product default must be the neutral modelling view.
     *
     * <p>Index 0 is what native code starts at, so "Studio Solid is 0" and
     * "Studio Solid is the default" are the same fact — which is exactly why
     * the debug source-colour path must not be 0.
     */
    @Test
    public void theDefaultShadingModelIsNotADiagnostic() {
        assertTrue("a freshly launched viewport must not come up in a debug mode",
                NativeViewport.SHADING_DEBUG_SOURCE_COLOR != 0);
        assertEquals("the default shading model is Studio Solid",
                NativeViewport.SHADING_STUDIO, 0);
        assertEquals("the default surface shading is Smooth",
                NativeViewport.SURFACE_SMOOTH, 0);
    }
}
